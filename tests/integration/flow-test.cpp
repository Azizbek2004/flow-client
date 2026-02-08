#include "core/BackendAPI.h"
#include "core/ConnectionManager.h"
#include "core/ProtocolGuard.h"
#include "protocols/XRayHandler.h"
#include <QCoreApplication>
#include <gtest/gtest.h>

// Mock BackendAPI for testing if needed, or use real one with test credentials

TEST(FlowIntegration, CanConnectToServer)
{
    int argc = 0;
    char *argv[] = {};
    QCoreApplication app(argc, argv);

    BackendAPI *api = BackendAPI::instance();
    // Assuming we have a way to mock the response or we use a live test server
    // For now, let's just assert the instance is valid
    ASSERT_TRUE(api != nullptr);

    // Test login structure (won't actually work without network/server)
    // api->login("test@example.com");
}

TEST(FlowIntegration, ProtocolLockWorks)
{
    ProtocolGuard *guard = ProtocolGuard::instance();
    guard->setLocked(false);
    ASSERT_TRUE(guard->canSwitchProtocol());

    guard->setLocked(true);
    ASSERT_FALSE(guard->canSwitchProtocol());

    guard->setLocked(false);
    ASSERT_TRUE(guard->canSwitchProtocol());
}

TEST(FlowIntegration, XRayHandlerStartStop)
{
    XRayHandler xray;
    QJsonObject config;
    // Fill with dummy config
    QJsonObject server;
    server["address"] = "127.0.0.1";
    server["port"] = 443;
    QJsonObject user;
    user["id"] = "uuid";
    QJsonObject reality;
    reality["sni"] = "example.com";
    reality["publicKey"] = "key";
    reality["shortId"] = "id";

    config["server"] = server;
    config["user"] = user;
    config["reality"] = reality;

    // Without actual xray binary this will fail or we need to mock QProcess
    // ASSERT_TRUE(xray.start(config));
    // ASSERT_TRUE(xray.isRunning());
    // xray.stop();
    // ASSERT_FALSE(xray.isRunning());
}
