#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <condition_variable>
#include <atomic>

#include <microhttpd.h>

namespace httpmock
{

enum class HttpMethod
{
    Get,
    PostFormUrlEncoded,
    PostMultipart,
    PostRawData
};

class HttpMockServer;
class ConnectionData
{
public:
    HttpMockServer *mockServer;
    MHD_Connection *connection;
    MHD_PostProcessor *postProcessor;

    // request data
    std::string url;
    std::string version;
    HttpMethod httpMethod;
    std::unordered_map<std::string, std::string> urlArguments;
    std::unordered_map<std::string, std::string> header;

    // request data (PostMultipart)
    std::string postKey;
    std::string postFileName;
    std::string postContentType;
    std::string postTransferEncoding;

    // request data (PostFormUrlEncoded)
    std::unordered_map<std::string, std::string> postUrlEncoded;

    // request data (PostMultipart + PostRawData)
    std::vector<std::byte> postData;

    // response data
    std::unordered_map<std::string, std::string> responseHeader;
    std::string responseBody;
    int responseCode;
};

using callbackFunction = std::function<void (ConnectionData *connectionData)>;

class HttpMockServer
{
public:
    explicit HttpMockServer(int port = 8080);
    ~HttpMockServer();
    void start();
    void stop();
    bool isRunning();

    bool waitForRequestCompleted(uint32_t count = 1, uint32_t timeoutMs = 0);
    ConnectionData *lastConnectionData();
    void setGenerateResponseCallback(const callbackFunction &newGenerateResponseCallback);

    int port() const;

private:
    // C-Callbacks from libmicrohttpd library
    static enum MHD_Result staticOnConnectionCallback(void *token, struct MHD_Connection *connection, const char *url, const char *method, const char *version, const char *uploadData, size_t *uploadDataSize, void **connectionToken);
    static enum MHD_Result staticOnIteratePostCallback(void *token, enum MHD_ValueKind kind, const char *key, const char *filename, const char *contentType, const char *transferEncoding, const char *data, uint64_t offset, size_t size);
    static void staticOnRequestCompleted(void *token, struct MHD_Connection *connection, void **connectionToken, enum MHD_RequestTerminationCode terminationCode);   
    static enum MHD_Result staticOnKeyValueIterator(void *token, enum MHD_ValueKind kind, const char *key, const char *value);

    enum MHD_Result onConnectionCallback(struct MHD_Connection *connection, const char *url, const char *method, const char *version, const char *uploadData, size_t *uploadDataSize, void **connectionToken);
    enum MHD_Result onIteratePostCallback(ConnectionData* connectionData, enum MHD_ValueKind kind, const char *key, const char *filename, const char *contentType, const char *transferEncoding, const char *data, uint64_t offset, size_t size);
    void onRequestCompleted(struct MHD_Connection *connection, void **connectionToken, enum MHD_RequestTerminationCode terminationCode);

    MHD_Result generateResponse(ConnectionData *connectionData);

    std::unique_ptr<MHD_Daemon, void(*)(MHD_Daemon*)> m_httpServer;
    std::vector<std::unique_ptr<ConnectionData>> m_runningConnections;
    std::unique_ptr<ConnectionData> m_lastConnection;

    callbackFunction m_generateResponseCallback;

    // See for details: https://www.modernescpp.com/index.php/c-core-guidelines-be-aware-of-the-traps-of-condition-variables
    bool m_requestCompletedPredicate{false};
    std::mutex m_requestCompletedMutex;
    std::condition_variable m_requestCompletedConditionVariable;
    int m_port;
    std::atomic<bool> m_callbackRunning{false};
};

}
