#include "testenvironment.hpp"

#include <gmock/gmock.h>
#include <curl/curl.h>

static httpmock::TestEnvironment* mockServerEnv = nullptr;

TEST(TestEnvironmentAutoPort, Valid)
{
    EXPECT_NE(mockServerEnv, nullptr);

    const int port = mockServerEnv->getMock()->port();
    EXPECT_TRUE((port >= 9200) && (port <= 9300));

    EXPECT_TRUE(mockServerEnv->getMock()->isRunning());
}

TEST(TestEnvironmentAutoPort, PostFormUrlEncoded)
{
    std::string url = "/post-form-url";
    const char urlFields[] = "name=daniel&project=curl";

    CURL *curlHandle = curl_easy_init();
    EXPECT_NE(curlHandle, nullptr);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(mockServerEnv->getMock()->port()) + url;
    curl_easy_setopt(curlHandle, CURLOPT_URL, requestUrl.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, urlFields);

    CURLcode returnCode = curl_easy_perform(curlHandle);
    EXPECT_EQ(returnCode, CURLE_OK) << curl_easy_strerror(returnCode);

    long httpResponseCode;
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &httpResponseCode);
    curl_easy_cleanup(curlHandle);
    EXPECT_TRUE(mockServerEnv->getMock()->waitForRequestCompleted(1, 1000));

    EXPECT_EQ(httpResponseCode, 200);
    EXPECT_TRUE(mockServerEnv->getMock()->lastConnectionData()->url == url);
    EXPECT_EQ(mockServerEnv->getMock()->lastConnectionData()->httpMethod, httpmock::HttpMethod::PostFormUrlEncoded);

    EXPECT_EQ(mockServerEnv->getMock()->lastConnectionData()->postUrlEncoded.size(), 2);
    EXPECT_TRUE(mockServerEnv->getMock()->lastConnectionData()->postUrlEncoded["name"]    == "daniel");
    EXPECT_TRUE(mockServerEnv->getMock()->lastConnectionData()->postUrlEncoded["project"] == "curl");
}

int main(int argc, char *argv[])
{
    curl_global_init(CURL_GLOBAL_ALL);

    ::testing::InitGoogleTest(&argc, argv);
    ::testing::Environment * const env = ::testing::AddGlobalTestEnvironment(httpmock::createMockServerEnvironment(9200));
    mockServerEnv = dynamic_cast<httpmock::TestEnvironment*>(env);
    std::cout << "mock server listening on port: " << std::to_string(mockServerEnv->getMock()->port()) << std::endl;
    int returnValue = RUN_ALL_TESTS();

    curl_global_cleanup();
    return returnValue;
}
