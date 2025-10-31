#include "FileServiceImpl.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>

using securecloud::files::DeleteRequest;
using securecloud::files::DeleteResponse;
using securecloud::files::DownloadChunk;
using securecloud::files::DownloadRequest;
using securecloud::files::FileMetadata;
using securecloud::files::MetadataRequest;
using securecloud::files::UploadAck;
using securecloud::files::UploadChunk;

namespace {
constexpr std::size_t kStreamChunkSize = 64 * 1024; // 64 KiB
}

FileServiceImpl::FileServiceImpl(std::string storage_root)
    : storage_root_(std::move(storage_root)) {
    std::error_code ec;
    std::filesystem::create_directories(storage_root_, ec);
    if (ec) {
        throw std::runtime_error("Unable to create storage directory: " + ec.message());
    }
}

std::string FileServiceImpl::generateFileId() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static constexpr char kAlphabet[] = "0123456789abcdef";
    std::uniform_int_distribution<int> dist(0, 15);
    std::string id = "file_";
    for (int i = 0; i < 32; ++i) {
        id.push_back(kAlphabet[dist(rng)]);
    }
    return id;
}

std::string FileServiceImpl::makeFilePath(const std::string& file_id) const {
    return (std::filesystem::path(storage_root_) / (file_id + ".bin")).string();
}

FileMetadata FileServiceImpl::toProto(const StoredFile& file) const {
    FileMetadata proto;
    proto.set_file_id(file.file_id);
    proto.set_conversation_id(file.conversation_id);
    proto.set_owner_id(file.owner_id);
    proto.set_file_name(file.file_name);
    proto.set_media_type(file.media_type);
    proto.set_size_bytes(file.size_bytes);
    proto.set_created_at_epoch(file.created_at_epoch);
    proto.set_deleted(file.deleted);
    return proto;
}

grpc::Status FileServiceImpl::Upload(grpc::ServerContext* /*context*/,
                                     grpc::ServerReader<UploadChunk>* reader,
                                     UploadAck* response) {
    UploadChunk chunk;
    bool has_data = false;
    bool last_chunk_seen = false;
    StoredFile stored;
    std::ofstream output;

    std::int64_t declared_size = -1;
    while (reader->Read(&chunk)) {
        if (!has_data) {
            if (chunk.file_name().empty()) {
                return {grpc::StatusCode::INVALID_ARGUMENT, "file_name is required"};
            }
            if (chunk.owner_id().empty()) {
                return {grpc::StatusCode::INVALID_ARGUMENT, "owner_id is required"};
            }
            stored.file_id = generateFileId();
            stored.conversation_id = chunk.conversation_id();
            stored.owner_id = chunk.owner_id();
            stored.file_name = chunk.file_name();
            stored.media_type = chunk.media_type();
            stored.created_at_epoch = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            stored.file_path = makeFilePath(stored.file_id);

            output.open(stored.file_path, std::ios::binary);
            if (!output.is_open()) {
                return {grpc::StatusCode::INTERNAL, "Cannot open storage path"};
            }
            has_data = true;
        }

        if (!chunk.cipher_chunk().empty()) {
            output.write(chunk.cipher_chunk().data(), static_cast<std::streamsize>(chunk.cipher_chunk().size()));
            if (!output) {
                output.close();
                std::filesystem::remove(stored.file_path);
                return {grpc::StatusCode::INTERNAL, "Failed writing to storage"};
            }
            stored.size_bytes += static_cast<std::int64_t>(chunk.cipher_chunk().size());
        }

            if (chunk.declared_size() > 0) {
            declared_size = chunk.declared_size();
        }

        if (chunk.last_chunk()) {
            last_chunk_seen = true;
        }
    }

    if (!has_data) {
        return {grpc::StatusCode::INVALID_ARGUMENT, "No data received"};
    }

    output.close();

    if (!last_chunk_seen) {
        std::filesystem::remove(stored.file_path);
        return {grpc::StatusCode::INVALID_ARGUMENT, "Stream ended before last chunk"};
    }

    if (declared_size >= 0 && declared_size != stored.size_bytes) {
        std::filesystem::remove(stored.file_path);
        return {grpc::StatusCode::INVALID_ARGUMENT, "Declared size mismatch"};
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        files_[stored.file_id] = stored;
    }

    response->set_file_id(stored.file_id);
    response->set_stored_size(stored.size_bytes);
    return grpc::Status::OK;
}

grpc::Status FileServiceImpl::Download(grpc::ServerContext* /*context*/,
                                       const DownloadRequest* request,
                                       grpc::ServerWriter<DownloadChunk>* writer) {
    if (request->file_id().empty()) {
        return {grpc::StatusCode::INVALID_ARGUMENT, "file_id is required"};
    }

    StoredFile file;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = files_.find(request->file_id());
        if (it == files_.end() || it->second.deleted) {
            return {grpc::StatusCode::NOT_FOUND, "Unknown file"};
        }
        file = it->second;
    }

    std::ifstream input(file.file_path, std::ios::binary);
    if (!input.is_open()) {
        return {grpc::StatusCode::INTERNAL, "File missing on disk"};
    }

    std::vector<char> buffer(kStreamChunkSize);
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        std::streamsize read_bytes = input.gcount();
        if (read_bytes <= 0) break;

        DownloadChunk chunk;
        chunk.set_cipher_chunk(buffer.data(), static_cast<int>(read_bytes));
        chunk.set_last_chunk(input.peek() == std::char_traits<char>::eof());
        if (!writer->Write(chunk)) {
            break;
        }
    }

    return grpc::Status::OK;
}

grpc::Status FileServiceImpl::Delete(grpc::ServerContext* /*context*/,
                                     const DeleteRequest* request,
                                     DeleteResponse* response) {
    if (request->file_id().empty()) {
        return {grpc::StatusCode::INVALID_ARGUMENT, "file_id is required"};
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = files_.find(request->file_id());
    if (it == files_.end() || it->second.deleted) {
        response->set_success(false);
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Unknown file");
    }

    // Autorisation basique : seul le propriétaire peut supprimer
    if (!request->requester_id().empty() && request->requester_id() != it->second.owner_id) {
        response->set_success(false);
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Not owner of file");
    }

    std::error_code ec;
    std::filesystem::remove(it->second.file_path, ec);
    it->second.deleted = true;
    response->set_success(true);
    return grpc::Status::OK;
}

grpc::Status FileServiceImpl::GetMetadata(grpc::ServerContext* /*context*/,
                                          const MetadataRequest* request,
                                          FileMetadata* response) {
    if (request->file_id().empty()) {
        return {grpc::StatusCode::INVALID_ARGUMENT, "file_id is required"};
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = files_.find(request->file_id());
    if (it == files_.end() || it->second.deleted) {
        return {grpc::StatusCode::NOT_FOUND, "Unknown file"};
    }

    *response = toProto(it->second);
    return grpc::Status::OK;
}
