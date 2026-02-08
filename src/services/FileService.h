#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>

/**
 * @brief Service de gestion de fichiers
 * 
 * Ce service communiquera avec le microservice file_service via gRPC.
 * Pour l'instant, c'est un stub.
 */
class FileService : public QObject {
    Q_OBJECT
    
public:
    static FileService& instance() {
        static FileService instance;
        return instance;
    }
    
    /**
     * @brief Upload un fichier
     */
    void uploadFile(const QString& filePath, const QString& recipientId) {
        // TODO: Implémenter l'appel gRPC vers file_service
        Q_UNUSED(filePath);
        Q_UNUSED(recipientId);
        
        emit fileUploaded(filePath);
    }
    
    /**
     * @brief Télécharge un fichier
     */
    void downloadFile(const QString& fileId, const QString& savePath) {
        // TODO: Implémenter l'appel gRPC
        Q_UNUSED(fileId);
        Q_UNUSED(savePath);
        
        emit fileDownloaded(fileId, savePath);
    }
    
signals:
    void fileUploaded(const QString& filePath);
    void fileDownloaded(const QString& fileId, const QString& savePath);
    void uploadProgress(int percentage);
    void downloadProgress(int percentage);
    void fileError(const QString& error);
    
private:
    FileService() = default;
    ~FileService() = default;
    FileService(const FileService&) = delete;
    FileService& operator=(const FileService&) = delete;
};
