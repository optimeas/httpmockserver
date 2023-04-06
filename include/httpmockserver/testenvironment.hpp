#pragma once

#include "httpmockserver.hpp"

#include <gtest/gtest.h>

namespace httpmock
{

std::unique_ptr<HttpMockServer> getFirstRunningMockServer(unsigned port = 8080, unsigned tryCount = 1000);
::testing::Environment* createMockServerEnvironment(unsigned startPort = 8080, unsigned tryCount = 1000);

class TestEnvironment : public ::testing::Environment
{
public:
    explicit TestEnvironment(int port = 8080);
    explicit TestEnvironment(std::unique_ptr<HttpMockServer>&& mock);
    const std::unique_ptr<HttpMockServer>& getMock() const;

    virtual void SetUp() override;
    virtual void TearDown() override;

private:
    std::unique_ptr<HttpMockServer> m_httpServer;
};

}
