#include "include/httpmockserver/httpmockserver.hpp"
#include "cpp-utils/scope_guard.hpp"

#include <iostream>
#include <thread>

#include <chrono>
#include <string.h>

namespace httpmock
{

HttpMockServer::HttpMockServer(int port)
 : m_httpServer(nullptr, &MHD_stop_daemon)
 , m_port(port)
{

}

HttpMockServer::~HttpMockServer()
{
    while(m_callbackRunning)
    {
        std::cout << "destructor waiting for running callback in other thread ..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void HttpMockServer::start()
{
    m_httpServer.reset(MHD_start_daemon(MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD, m_port, NULL, NULL,
        &staticOnConnectionCallback, this, MHD_OPTION_NOTIFY_COMPLETED, staticOnRequestCompleted, this, MHD_OPTION_END));

    if(!m_httpServer)
        throw std::runtime_error("HttpMockServer has failed to start!");
}

void HttpMockServer::stop()
{
    m_httpServer.reset();
}

bool HttpMockServer::isRunning()
{
    return m_httpServer != nullptr;
}

bool HttpMockServer::waitForRequestCompleted(uint32_t count, uint32_t timeoutMs)
{
    for(uint32_t i=0; i<count; ++i)
    {
        bool success = true;
        {
            std::unique_lock<std::mutex> lock(m_requestCompletedMutex);
            if(timeoutMs == 0)
                m_requestCompletedConditionVariable.wait(lock, [this]{ return m_requestCompletedPredicate; });
            else
                success = m_requestCompletedConditionVariable.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]{ return m_requestCompletedPredicate; });
        }

        if(!success)
            return false;

        {
            // For multiple runs, the flag must be reset here.
            // But it should happen after the wait in any case, to avoid a kind of deadlock if the answer has already arrived through synchronous calls, and only afterwards this function is called.
            std::lock_guard<std::mutex> lock(m_requestCompletedMutex);
            m_requestCompletedPredicate = false;
        }
    }

    return true;
}

ConnectionData *HttpMockServer::lastConnectionData()
{
    return m_lastConnection.get();
}

MHD_Result HttpMockServer::staticOnConnectionCallback(void *token, MHD_Connection *connection, const char *url, const char *method, const char *version, const char *uploadData, size_t *uploadDataSize, void **connectionToken)
{
    if(token != nullptr)
        return static_cast<HttpMockServer*>(token)->onConnectionCallback(connection, url, method, version, uploadData, uploadDataSize, connectionToken);
    else
        return MHD_NO;
}

MHD_Result HttpMockServer::staticOnIteratePostCallback(void *token, MHD_ValueKind kind, const char *key, const char *filename, const char *contentType, const char *transferEncoding, const char *data, uint64_t offset, size_t size)
{
    if(token != nullptr)
    {
        ConnectionData *connectionData = static_cast<ConnectionData*>(token);
        return connectionData->mockServer->onIteratePostCallback(connectionData, kind, key, filename, contentType, transferEncoding, data, offset, size);
    }
    else
        return MHD_NO;
}

void HttpMockServer::staticOnRequestCompleted(void *token, MHD_Connection *connection, void **connectionToken, MHD_RequestTerminationCode terminationCode)
{
    if(token != nullptr)
        return static_cast<HttpMockServer*>(token)->onRequestCompleted(connection, connectionToken, terminationCode);
}

MHD_Result HttpMockServer::staticOnKeyValueIterator(void *token, [[maybe_unused]] MHD_ValueKind kind, const char *key, const char *value)
{

    if(token && key && value)
    {
        std::unordered_map<std::string, std::string> *container = static_cast<std::unordered_map<std::string, std::string> *>(token);;
        container->operator [](key) = value;
    }

    return MHD_YES;
}

MHD_Result HttpMockServer::onConnectionCallback(MHD_Connection *connection, const char *url, const char *method, const char *version, const char *uploadData, size_t *uploadDataSize, void **connectionToken)
{
    m_callbackRunning = true;
    CU_SCOPE_EXIT{m_callbackRunning = false;};

    // This function is called multiple times during one HTTP request
    if(*connectionToken == nullptr)
    {
        {
            std::lock_guard<std::mutex> lock(m_requestCompletedMutex);
            m_requestCompletedPredicate = false;
        }

        // first time we arrive here
        std::unique_ptr<ConnectionData> connectionData = std::make_unique<ConnectionData>();

        connectionData->mockServer = this;
        connectionData->connection = connection;
        connectionData->responseCode = MHD_HTTP_OK;

        if(url)
            connectionData->url = url;

        if(version)
            connectionData->version = version;

        if(strcmp(method, "POST") == 0)
        {
            connectionData->postProcessor = MHD_create_post_processor(connection, 65536, staticOnIteratePostCallback, static_cast<void *>(connectionData.get()));
            if(connectionData->postProcessor == nullptr)
                connectionData->httpMethod = HttpMethod::PostRawData;
            else
                connectionData->httpMethod = HttpMethod::PostFormUrlEncoded; // can also be HttpMethod::PostMultipart; will be evaluated later
        }
        else
            connectionData->httpMethod = HttpMethod::Get;

        *connectionToken = static_cast<void *>(connectionData.get());
        m_runningConnections.push_back(std::move(connectionData));
        return MHD_YES;
    }

    if(strcmp(method, "GET") == 0)
    {
        // third time we arrive here (GET)
        ConnectionData *connectionData = static_cast<ConnectionData*>(*connectionToken);
        return generateResponse(connectionData);
    }

    if(strcmp(method, "POST") == 0)
    {
        ConnectionData *connectionData = static_cast<ConnectionData*>(*connectionToken);
        if((uploadDataSize != nullptr) && (*uploadDataSize != 0))
        {
            // second time we arrive here
            if(connectionData->httpMethod == HttpMethod::PostRawData)
            {
                size_t dataSize = *uploadDataSize;
                std::byte *dataPointer = reinterpret_cast<std::byte*>(const_cast<char *>(uploadData));
                connectionData->postData.insert(connectionData->postData.end(), dataPointer, dataPointer + dataSize);
            }
            else
                MHD_post_process(connectionData->postProcessor, uploadData, *uploadDataSize);

            *uploadDataSize = 0;
            return MHD_YES;
        }
        else
        {
            // third time we arrive here (POST)
            return generateResponse(connectionData);
        }
    }

    // we should not arrive here ...
    struct MHD_Response *response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
    if(!response)
        return MHD_NO;

    enum MHD_Result returnCode = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
    MHD_destroy_response(response);
    return returnCode;
}

MHD_Result HttpMockServer::onIteratePostCallback(ConnectionData* connectionData, [[maybe_unused]] MHD_ValueKind kind, const char *key, const char *filename, const char *contentType, const char *transferEncoding, const char *data, uint64_t offset, size_t size)
{
    m_callbackRunning = true;
    CU_SCOPE_EXIT{m_callbackRunning = false;};

    if((filename == nullptr) && (contentType == nullptr))
    {
        connectionData->httpMethod = HttpMethod::PostFormUrlEncoded;
        if(key && data)
            connectionData->postUrlEncoded[key] = data;
    }
    else
    {
        connectionData->httpMethod = HttpMethod::PostMultipart;

        if(offset == 0) // start of new multipart
        {
            // Currently we support only one (the last) multipart
            if(key)
                connectionData->postKey = key;
            else
                connectionData->postKey.clear();

            if(filename)
                connectionData->postFileName = filename;
            else
                connectionData->postFileName.clear();

            if(contentType)
                connectionData->postContentType = contentType;
            else
                connectionData->postContentType.clear();

            if(transferEncoding)
                connectionData->postTransferEncoding = transferEncoding;
            else
                connectionData->postTransferEncoding.clear();

            connectionData->postData.clear();
        }

        if(size)
        {
            std::byte *dataPointer = reinterpret_cast<std::byte*>(const_cast<char *>(data));
            connectionData->postData.insert(connectionData->postData.end(), dataPointer, dataPointer + size);
        }
    }

    return MHD_YES;
}

void HttpMockServer::onRequestCompleted([[maybe_unused]] MHD_Connection *connection, void **connectionToken, [[maybe_unused]] MHD_RequestTerminationCode terminationCode)
{
    m_callbackRunning = true;
    CU_SCOPE_EXIT{m_callbackRunning = false;};

    ConnectionData *connectionData = static_cast<ConnectionData *>(*connectionToken);
    if(connectionData == nullptr)
        return;

    if(    (connectionData->httpMethod == HttpMethod::PostFormUrlEncoded)
        || (connectionData->httpMethod == HttpMethod::PostMultipart))
    {
        MHD_destroy_post_processor(connectionData->postProcessor);
    }

    auto savedConnectionData = std::find_if(m_runningConnections.begin(), m_runningConnections.end(), [connectionData](std::unique_ptr<ConnectionData> &entry)
    {
        return connectionData == entry.get();
    });

    if(savedConnectionData != m_runningConnections.end())
    {
        m_lastConnection = std::move(*savedConnectionData);
        m_runningConnections.erase(savedConnectionData);
        *connectionToken = nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(m_requestCompletedMutex);
        m_requestCompletedPredicate = true;
    }
    m_requestCompletedConditionVariable.notify_one();
}

MHD_Result HttpMockServer::generateResponse(ConnectionData *connectionData)
{
    MHD_get_connection_values(connectionData->connection, MHD_GET_ARGUMENT_KIND, &staticOnKeyValueIterator, &connectionData->urlArguments);
    MHD_get_connection_values(connectionData->connection, MHD_HEADER_KIND,       &staticOnKeyValueIterator, &connectionData->header);

    if(m_generateResponseCallback)
        m_generateResponseCallback(connectionData);

    struct MHD_Response *response;
    if(connectionData->responseBody.size() > 0)
        response = MHD_create_response_from_buffer(connectionData->responseBody.size(), const_cast<char *>(connectionData->responseBody.c_str()), MHD_RESPMEM_PERSISTENT);
    else
        response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
    if(!response)
        return MHD_NO;

    for(auto &entry : connectionData->responseHeader)
    {
        enum MHD_Result returnCode = MHD_add_response_header(response, entry.first.c_str(), entry.second.c_str());
        if(returnCode == MHD_NO)
            return MHD_NO;
    }

    enum MHD_Result returnCode = MHD_queue_response(connectionData->connection, connectionData->responseCode, response);
    MHD_destroy_response(response);
    return returnCode;
}

int HttpMockServer::port() const
{
    return m_port;
}

void HttpMockServer::setGenerateResponseCallback(const callbackFunction &newGenerateResponseCallback)
{
    m_generateResponseCallback = newGenerateResponseCallback;
}

}
