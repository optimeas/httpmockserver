#include "httpmockserver/httpmockserver.hpp"

#include <string>
#include <iostream>
#include <cstring>
#include <regex>

#include <gmock/gmock.h>
#include <curl/curl.h>

int port = 57567;

std::string receiveBuffer;
static size_t CurlWriteMemoryCallback(void *contents, size_t size, size_t nmemb, [[maybe_unused]] void *userp)
{
    size_t realsize = size * nmemb;
    char * dataPointer = static_cast<char *>(contents);
    receiveBuffer.append(dataPointer, dataPointer + realsize);
    return realsize;
}

std::unordered_map<std::string, std::string> receiveHeaders;
static size_t CurlHeaderCallback(char* buffer, size_t size, size_t nitems, [[maybe_unused]] void* userdata)
{
    std::string line;
    line.append(buffer, nitems * size);

    auto const regexSplit = std::regex(R"(([\w-]+): *([[:print:]]+))");
    std::smatch matches;
    if(std::regex_search(line, matches, regexSplit))
    {
        if(matches.size() == 3)
            receiveHeaders[matches[1].str()] = matches[2].str();
    }

    return nitems * size;
}

TEST(HttpMockServer, Get)
{
    receiveBuffer.clear();
    receiveHeaders.clear();
    std::string url = "/get-url";
    std::string response = "<html><body>HttpMockServer</body></html>";

    httpmock::HttpMockServer mockServer(port);
    mockServer.setGenerateResponseCallback([&](httpmock::ConnectionData *connectionData)
    {
        connectionData->responseHeader["Content-Description"] = "Message-ID: 152018";
        connectionData->responseHeader["Content-Type"] = "application/om-scpi-app";
        connectionData->responseBody = response;
        connectionData->responseCode = 200;
    });

    mockServer.start();
    EXPECT_TRUE(mockServer.isRunning());

    CURL *curlHandle = curl_easy_init();
    EXPECT_NE(curlHandle, nullptr);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    curl_easy_setopt(curlHandle, CURLOPT_URL, requestUrl.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, CurlWriteMemoryCallback);
    curl_easy_setopt(curlHandle, CURLOPT_HEADERFUNCTION, CurlHeaderCallback);

    struct curl_slist *headerList = NULL;
    headerList = curl_slist_append(headerList, "Content-Description: Message-ID: 152019, File-Tag: preview");
    curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headerList);

    CURLcode returnCode = curl_easy_perform(curlHandle);
    EXPECT_EQ(returnCode, CURLE_OK) << curl_easy_strerror(returnCode);

    long httpResponseCode;
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &httpResponseCode);
    curl_easy_cleanup(curlHandle);
    curl_slist_free_all(headerList);
    EXPECT_TRUE(mockServer.waitForRequestCompleted(1, 1000));

    EXPECT_EQ(httpResponseCode, 200);
    EXPECT_TRUE(receiveBuffer == response);
    EXPECT_TRUE(receiveHeaders["Content-Description"] == "Message-ID: 152018");
    EXPECT_TRUE(receiveHeaders["Content-Type"]        == "application/om-scpi-app");

    EXPECT_TRUE(mockServer.lastConnectionData()->url == url);
    EXPECT_EQ(  mockServer.lastConnectionData()->httpMethod, httpmock::HttpMethod::Get);
    EXPECT_TRUE(mockServer.lastConnectionData()->header["Content-Description"] == "Message-ID: 152019, File-Tag: preview");
}

TEST(HttpMockServer, PostFormUrlEncoded)
{
    std::string url = "/post-form-url";
    char urlFields[] = "name=daniel&project=curl";

    httpmock::HttpMockServer mockServer(port);
    mockServer.start();
    EXPECT_TRUE(mockServer.isRunning());

    CURL *curlHandle = curl_easy_init();
    EXPECT_NE(curlHandle, nullptr);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    curl_easy_setopt(curlHandle, CURLOPT_URL, requestUrl.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, urlFields);

    CURLcode returnCode = curl_easy_perform(curlHandle);
    EXPECT_EQ(returnCode, CURLE_OK) << curl_easy_strerror(returnCode);

    long httpResponseCode;
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &httpResponseCode);
    curl_easy_cleanup(curlHandle);
    EXPECT_TRUE(mockServer.waitForRequestCompleted(1, 1000));

    EXPECT_EQ(httpResponseCode, 200);
    EXPECT_TRUE(mockServer.lastConnectionData()->url == url);
    EXPECT_EQ(mockServer.lastConnectionData()->httpMethod, httpmock::HttpMethod::PostFormUrlEncoded);

    EXPECT_EQ(mockServer.lastConnectionData()->postUrlEncoded.size(), 2);
    EXPECT_TRUE(mockServer.lastConnectionData()->postUrlEncoded["name"]    == "daniel");
    EXPECT_TRUE(mockServer.lastConnectionData()->postUrlEncoded["project"] == "curl");
}

TEST(HttpMockServer, PostMultipart)
{
    std::string url = "/post-multipart-url";

    std::string content;
    content += R"(<?xml version="1.0" encoding="UTF-8"?>\n)";
    content += R"(<root>\n)";
    content += R"(    <attention text="File=/sde/preview/20220629/20220629_115750.osfz Tag=preview"/>\n)";
    content += R"(</root>)";

    httpmock::HttpMockServer mockServer(port);
    mockServer.start();
    EXPECT_TRUE(mockServer.isRunning());

    CURL *curlHandle = curl_easy_init();
    EXPECT_NE(curlHandle, nullptr);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    curl_easy_setopt(curlHandle, CURLOPT_URL, requestUrl.c_str());

    curl_mime *multiPartContainer = curl_mime_init(curlHandle);
    curl_mimepart *multiPartEntry = curl_mime_addpart(multiPartContainer);
    curl_mime_name(multiPartEntry, "data");
    curl_mime_data(multiPartEntry, content.c_str(), CURL_ZERO_TERMINATED);
    curl_mime_filename(multiPartEntry, "omCloudService-0.xml");
    curl_mime_type(multiPartEntry, "image/xml");
    curl_easy_setopt(curlHandle, CURLOPT_MIMEPOST, multiPartContainer);

    CURLcode returnCode = curl_easy_perform(curlHandle);
    EXPECT_EQ(returnCode, CURLE_OK) << curl_easy_strerror(returnCode);

    long httpResponseCode;
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &httpResponseCode);
    curl_easy_cleanup(curlHandle);
    curl_mime_free(multiPartContainer);
    EXPECT_TRUE(mockServer.waitForRequestCompleted(1, 1000));

    EXPECT_EQ(httpResponseCode, 200);
    EXPECT_TRUE(mockServer.lastConnectionData()->url == url);
    EXPECT_EQ(mockServer.lastConnectionData()->httpMethod, httpmock::HttpMethod::PostMultipart);

    EXPECT_TRUE(mockServer.lastConnectionData()->postKey == "data");
    EXPECT_TRUE(mockServer.lastConnectionData()->postFileName == "omCloudService-0.xml");
    EXPECT_TRUE(mockServer.lastConnectionData()->postContentType == "image/xml");

    EXPECT_EQ(content.size(), mockServer.lastConnectionData()->postData.size());
    EXPECT_EQ(std::memcmp(mockServer.lastConnectionData()->postData.data(), content.c_str(), content.size()), 0);
}

TEST(HttpMockServer, PostRawData)
{
    std::string url = "/post-raw-url";

    std::string content;
    content += R"(<?xml version="1.0" encoding="UTF-8"?>\n)";
    content += R"(<root>\n)";
    content += R"(    <attention text="File=/sde/preview/20220629/20220629_115750.osfz Tag=preview"/>\n)";
    content += R"(</root>)";

    httpmock::HttpMockServer mockServer(port);
    mockServer.start();
    EXPECT_TRUE(mockServer.isRunning());

    CURL *curlHandle = curl_easy_init();
    EXPECT_NE(curlHandle, nullptr);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    curl_easy_setopt(curlHandle, CURLOPT_URL, requestUrl.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, content.c_str());

    struct curl_slist *headerList=NULL;
    headerList = curl_slist_append(headerList, "Content-Type: application/json");
    curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headerList);

    CURLcode returnCode = curl_easy_perform(curlHandle);
    EXPECT_EQ(returnCode, CURLE_OK) << curl_easy_strerror(returnCode);

    long httpResponseCode;
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &httpResponseCode);
    curl_easy_cleanup(curlHandle);
    curl_slist_free_all(headerList);
    EXPECT_TRUE(mockServer.waitForRequestCompleted(1, 1000));

    EXPECT_EQ(httpResponseCode, 200);
    EXPECT_TRUE(mockServer.lastConnectionData()->url == url);
    EXPECT_EQ(mockServer.lastConnectionData()->httpMethod, httpmock::HttpMethod::PostRawData);

    EXPECT_EQ(content.size(), mockServer.lastConnectionData()->postData.size());
    EXPECT_EQ(std::memcmp(mockServer.lastConnectionData()->postData.data(), content.c_str(), content.size()), 0);
}

int main(int argc, char *argv[])
{
    curl_global_init(CURL_GLOBAL_ALL);

    ::testing::InitGoogleTest(&argc, argv);
    int returnValue = RUN_ALL_TESTS();

    curl_global_cleanup();
    return returnValue;
}
