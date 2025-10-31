#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "file_service.grpc.pb.h"

using securecloud::files::DeleteRequest;
using securecloud::files::DeleteResponse;
using securecloud::files::DownloadChunk;
using securecloud::files::DownloadRequest;
using securecloud::files::FileMetadata;
using securecloud::files::FileService;
using securecloud::files::MetadataRequest;
using securecloud::files::UploadAck;
using securecloud::files::UploadChunk;

namespace {
std::string generatePayload(std::size_t size) {
    std::string payload;
    payload.resize(size);
    std::mt19937 rng(static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<int> dist(0, 255);
    for (auto& ch : payload) {
        ch = static_cast<char>(dist(rng));
    }
    return payload;
}
}

int main(int argc, char** argv) {
    std::string target = (argc > 1) ? argv[1] : "localhost:50052";
    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    auto stub = FileService::NewStub(channel);

    const std::string conversation_id = "conv-demo";
    const std::string owner_id = "user-42";
    const std::string file_name = "demo.bin";
    const std::string media_type = "application/octet-stream";
    const std::string payload = generatePayload(128 * 3 + 5); // 389 bytes

    // --- Upload ---
    grpc::ClientContext upload_ctx;
    UploadAck ack;
    auto writer = stub->Upload(&upload_ctx, &ack);

    const std::size_t chunk_size = 128;
    std::size_t offset = 0;
    while (offset < payload.size()) {
        UploadChunk chunk;
        chunk.set_conversation_id(conversation_id);
        chunk.set_owner_id(owner_id);
        chunk.set_file_name(file_name);
        chunk.set_media_type(media_type);
        chunk.set_declared_size(static_cast<std::int64_t>(payload.size()));

        std::size_t remaining = payload.size() - offset;
        std::size_t to_copy = std::min(chunk_size, remaining);
    chunk.set_cipher_chunk(payload.data() + offset, static_cast<int>(to_copy));
        offset += to_copy;
        chunk.set_last_chunk(offset >= payload.size());

        if (!writer->Write(chunk)) {
            std::cerr << "[FileServiceTest] Write failed during upload" << std::endl;
            return 1;
        }
    }
    writer->WritesDone();

    auto status = writer->Finish();
    if (!status.ok()) {
        std::cerr << "[FileServiceTest] Upload failed: " << status.error_message() << std::endl;
        return 1;
    }

    std::cout << "[FileServiceTest] Uploaded file_id=" << ack.file_id()
              << " stored_size=" << ack.stored_size() << std::endl;

    if (ack.stored_size() != static_cast<std::int64_t>(payload.size())) {
        std::cerr << "[FileServiceTest] Stored size mismatch" << std::endl;
        return 1;
    }

    const std::string file_id = ack.file_id();

    // --- Metadata ---
    grpc::ClientContext meta_ctx;
    MetadataRequest meta_req;
    meta_req.set_file_id(file_id);
    FileMetadata meta_resp;
    status = stub->GetMetadata(&meta_ctx, meta_req, &meta_resp);
    if (!status.ok()) {
        std::cerr << "[FileServiceTest] Metadata request failed: " << status.error_message() << std::endl;
        return 1;
    }

    if (meta_resp.file_name() != file_name || meta_resp.owner_id() != owner_id) {
        std::cerr << "[FileServiceTest] Metadata content mismatch" << std::endl;
        return 1;
    }

    // --- Download ---
    grpc::ClientContext download_ctx;
    DownloadRequest dl_req;
    dl_req.set_file_id(file_id);
    dl_req.set_requester_id(owner_id);

    std::unique_ptr<grpc::ClientReader<DownloadChunk>> reader = stub->Download(&download_ctx, dl_req);
    DownloadChunk dl_chunk;
    std::string reconstructed;
    while (reader->Read(&dl_chunk)) {
        reconstructed.append(dl_chunk.cipher_chunk());
    }
    status = reader->Finish();
    if (!status.ok()) {
        std::cerr << "[FileServiceTest] Download failed: " << status.error_message() << std::endl;
        return 1;
    }

    if (reconstructed != payload) {
        std::cerr << "[FileServiceTest] Reconstructed payload mismatch" << std::endl;
        return 1;
    }

    // --- Delete ---
    grpc::ClientContext delete_ctx;
    DeleteRequest del_req;
    del_req.set_file_id(file_id);
    del_req.set_requester_id(owner_id);
    DeleteResponse del_resp;
    status = stub->Delete(&delete_ctx, del_req, &del_resp);
    if (!status.ok() || !del_resp.success()) {
        std::cerr << "[FileServiceTest] Delete failed: " << status.error_message() << std::endl;
        return 1;
    }

    grpc::ClientContext meta_ctx_after;
    status = stub->GetMetadata(&meta_ctx_after, meta_req, &meta_resp);
    if (status.error_code() != grpc::StatusCode::NOT_FOUND) {
        std::cerr << "[FileServiceTest] Expected metadata to be gone" << std::endl;
        return 1;
    }

    std::cout << "[FileServiceTest] All scenarios succeeded" << std::endl;
    return 0;
}
