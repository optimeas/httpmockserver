#include "include/httpmockserver/testenvironment.hpp"

#include <iostream>

namespace httpmock
{

TestEnvironment::TestEnvironment(int port)
 : m_httpServer(std::make_unique<HttpMockServer>(port))
{
}

TestEnvironment::TestEnvironment(std::unique_ptr<HttpMockServer>&& mock)
 : m_httpServer(std::move(mock))
{
}

const std::unique_ptr<HttpMockServer> &TestEnvironment::getMock() const
{
    return m_httpServer;
}

void TestEnvironment::SetUp()
{
    if(!m_httpServer->isRunning())
        m_httpServer->start();
}

void TestEnvironment::TearDown()
{
    m_httpServer->stop();
}

std::unique_ptr<HttpMockServer> getFirstRunningMockServer(unsigned port, unsigned tryCount)
{
    for(unsigned p=0; p < tryCount; p++)
    {
        try
        {
            std::unique_ptr<HttpMockServer> server(std::make_unique<HttpMockServer>(port + p));
            server->start();
            return server;
        }
        catch(const std::runtime_error&)
        {
            continue;
        }
    }

    throw std::runtime_error("MockServer did not come up!");
}

::testing::Environment* createMockServerEnvironment(unsigned startPort, unsigned tryCount)
{
    return new TestEnvironment(getFirstRunningMockServer(startPort, tryCount));
}

}
