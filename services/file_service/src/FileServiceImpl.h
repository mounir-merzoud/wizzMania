#pragma once

#include <grpcpp/grpcpp.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "file_service.grpc.pb.h"

class FileServiceImpl final : public securecloud::files::FileService::Service {
public:
    explicit FileServiceImpl(std::string storage_root);

    grpc::Status Upload(grpc::ServerContext* context,
                        grpc::ServerReader<securecloud::files::UploadChunk>* reader,
                        securecloud::files::UploadAck* response) override;

    grpc::Status Download(grpc::ServerContext* context,
                          const securecloud::files::DownloadRequest* request,
                          grpc::ServerWriter<securecloud::files::DownloadChunk>* writer) override;

    grpc::Status Delete(grpc::ServerContext* context,
                        const securecloud::files::DeleteRequest* request,
                        securecloud::files::DeleteResponse* response) override;

    grpc::Status GetMetadata(grpc::ServerContext* context,
                             const securecloud::files::MetadataRequest* request,
                             securecloud::files::FileMetadata* response) override;

private:
    struct StoredFile {
        std::string file_id;
        std::string conversation_id;
        std::string owner_id;
        std::string file_name;
        std::string media_type;
        std::string file_path;
        std::int64_t size_bytes{0};
        std::int64_t created_at_epoch{0};
        bool deleted{false};
    };

    std::string generateFileId();
    std::string makeFilePath(const std::string& file_id) const;
    securecloud::files::FileMetadata toProto(const StoredFile& file) const;

    const std::string storage_root_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, StoredFile> files_;
};
