package com.wizzmania.messaging;

import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import io.grpc.stub.StreamObserver;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.Timeout;
import static org.junit.jupiter.api.Assertions.*;
import securecloud.messaging.EncryptedMessage;
import securecloud.messaging.HistoryRequest;
import securecloud.messaging.HistoryResponse;
import securecloud.messaging.MessagingServiceGrpc;
import securecloud.messaging.SendAck;

public class MessagingServiceIT {

    private static final Path REPO_ROOT = locateRepoRoot();
    private static final Path BUILD_DIR = REPO_ROOT.resolve("services/messaging_service/build");
    private static final Path SERVER_BINARY = BUILD_DIR.resolve("messaging_service");

    private Process serverProcess;
    private ManagedChannel channel;
    private MessagingServiceGrpc.MessagingServiceBlockingStub blockingStub;
    private MessagingServiceGrpc.MessagingServiceStub asyncStub;
    private int port;

    private static Path locateRepoRoot() {
        Path dir = Path.of(System.getProperty("user.dir"));
        while (dir != null && !Files.exists(dir.resolve("Makefile"))) {
            dir = dir.getParent();
        }
        if (dir == null) {
            throw new IllegalStateException("Impossible de localiser la racine du dépôt");
        }
        return dir;
    }

    @BeforeAll
    static void buildMessagingBinary() throws IOException, InterruptedException {
        ProcessBuilder pb = new ProcessBuilder("make", "build-messaging");
        pb.directory(REPO_ROOT.toFile());
        pb.inheritIO();
        Process make = pb.start();
        if (make.waitFor() != 0) {
            throw new IllegalStateException("make build-messaging a échoué");
        }
        if (!Files.exists(SERVER_BINARY)) {
            throw new IllegalStateException("Binaire messaging_service introuvable après compilation");
        }
    }

    @BeforeEach
    void startServer() throws Exception {
        this.port = findFreePort();
        ProcessBuilder pb = new ProcessBuilder(
                SERVER_BINARY.toAbsolutePath().toString(),
                "127.0.0.1:" + port
        );
        pb.directory(BUILD_DIR.toFile());
        pb.redirectErrorStream(true);
        this.serverProcess = pb.start();
        Thread.sleep(Duration.ofMillis(1200));
        if (!serverProcess.isAlive()) {
            throw new IllegalStateException("Le serveur messaging ne s'est pas lancé correctement");
        }
        this.channel = ManagedChannelBuilder.forAddress("127.0.0.1", port)
                .usePlaintext()
                .build();
        this.blockingStub = MessagingServiceGrpc.newBlockingStub(channel);
        this.asyncStub = MessagingServiceGrpc.newStub(channel);
    }

    @AfterEach
    void stopServer() throws Exception {
        if (channel != null) {
            channel.shutdownNow().awaitTermination(1, TimeUnit.SECONDS);
        }
        if (serverProcess != null && serverProcess.isAlive()) {
            serverProcess.destroy();
            if (!serverProcess.waitFor(1, TimeUnit.SECONDS)) {
                serverProcess.destroyForcibly();
            }
        }
    }

    @Test
    @Timeout(10)
    void sendMessageAndHistory_shouldPersistMessage() {
        EncryptedMessage request = EncryptedMessage.newBuilder()
                .setConversationId("conv-java")
                .setSenderId("java-client")
                .setCiphertext(com.google.protobuf.ByteString.copyFromUtf8("hello"))
                .setNonce(com.google.protobuf.ByteString.copyFromUtf8("n"))
                .build();

        SendAck ack = blockingStub.sendMessage(request);
        assertTrue(ack.getAccepted(), "Le message doit être accepté");
        assertFalse(ack.getMessageId().isEmpty(), "L'ack doit contenir un message_id");

        HistoryRequest historyRequest = HistoryRequest.newBuilder()
                .setConversationId("conv-java")
                .setLimit(5)
                .build();
        HistoryResponse history = blockingStub.getHistory(historyRequest);
        assertFalse(history.getMessagesList().isEmpty(), "L'historique ne doit pas être vide");
        assertTrue(
                history.getMessagesList().stream().anyMatch(m -> m.getMessageId().equals(ack.getMessageId())),
                "Le message envoyé doit apparaître dans l'historique"
        );
    }

    @Test
    @Timeout(10)
    void chatStream_shouldEchoMessages() throws InterruptedException {
        CountDownLatch latch = new CountDownLatch(1);
        List<EncryptedMessage> responses = new ArrayList<>();

        StreamObserver<EncryptedMessage> responseObserver = new StreamObserver<>() {
            @Override
            public void onNext(EncryptedMessage value) {
                responses.add(value);
                latch.countDown();
            }

            @Override
            public void onError(Throwable t) {
                latch.countDown();
                fail("Stream retourné en erreur: " + t.getMessage());
            }

            @Override
            public void onCompleted() {
                // rien
            }
        };

        StreamObserver<EncryptedMessage> requestObserver = asyncStub.chatStream(responseObserver);
        requestObserver.onNext(EncryptedMessage.newBuilder()
                .setConversationId("conv-stream")
                .setSenderId("java-client")
                .setCiphertext(com.google.protobuf.ByteString.copyFromUtf8("stream"))
                .setNonce(com.google.protobuf.ByteString.copyFromUtf8("n"))
                .build());
        requestObserver.onCompleted();

        assertTrue(latch.await(5, TimeUnit.SECONDS), "Aucune réponse reçue via ChatStream");
        assertFalse(responses.isEmpty());
        EncryptedMessage first = responses.get(0);
        assertEquals("conv-stream", first.getConversationId());
        assertEquals("java-client", first.getSenderId());
    }

    private static int findFreePort() throws IOException {
        try (var socket = new java.net.ServerSocket(0)) {
            socket.setReuseAddress(true);
            return socket.getLocalPort();
        }
    }
}
