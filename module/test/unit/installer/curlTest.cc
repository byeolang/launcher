// run against the faked libcurl in module/test/mock/, not the network. what is
// under test is the wrapper: its options, its tres, and what it leaves on disk.
#include "mock/curlMock.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace by;
using namespace std;
using ::testing::_;
using ::testing::Return;

namespace {
    struct curlTest: public ::testing::Test {};

    // most cases only care that a handle exists and gets released.
    void _expectHandleLifecycle(scopedCurlMock& mock) {
        EXPECT_CALL(*mock, easyInit()).WillOnce(Return(aCurlHandle()));
        EXPECT_CALL(*mock, easyCleanup(aCurlHandle()));
    }

    filesystem::path _aTempPath(const string& name) {
        filesystem::path path = filesystem::temp_directory_path() / name;
        error_code ec;
        filesystem::remove(path, ec);
        return path;
    }
}

TEST_F(curlTest, testValidWhenHandleIsAcquired) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);

    curl session;

    ASSERT_TRUE(session.isValid());
}

TEST_F(curlTest, testInvalidWhenHandleIsNotAcquired) {
    scopedCurlMock mock;
    EXPECT_CALL(*mock, easyInit()).WillOnce(Return(nullptr));
    // a handle that was never acquired must not be released.
    EXPECT_CALL(*mock, easyCleanup(_)).Times(0);

    curl session;

    ASSERT_FALSE(session.isValid());
}

TEST_F(curlTest, testDownloadAsStrFailsWithoutHandle) {
    scopedCurlMock mock;
    EXPECT_CALL(*mock, easyInit()).WillOnce(Return(nullptr));
    EXPECT_CALL(*mock, easyPerform(_)).Times(0);

    curl session;
    curl::res got = session.downloadAsStr("https://byeol.io/manifest");

    ASSERT_FALSE(got.has());
    ASSERT_EQ(got.getErr(), CURLE_FAILED_INIT);
}

TEST_F(curlTest, testDownloadAsStrGivesBackWholeContent) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);
    EXPECT_CALL(*mock, easyPerform(aCurlHandle())).WillOnce([&mock](CURL*) {
        mock->pump("hello manifest");
        return CURLE_OK;
    });

    curl session;
    curl::res got = session.downloadAsStr("https://byeol.io/manifest");

    ASSERT_TRUE(got.has());
    ASSERT_EQ(*got, "hello manifest");
}

TEST_F(curlTest, testDownloadAsStrAppendsEveryChunk) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);
    // libcurl hands a body over in as many callbacks as it likes.
    EXPECT_CALL(*mock, easyPerform(aCurlHandle())).WillOnce([&mock](CURL*) {
        mock->pump("first ");
        mock->pump("second");
        return CURLE_OK;
    });

    curl session;
    curl::res got = session.downloadAsStr("https://byeol.io/manifest");

    ASSERT_TRUE(got.has());
    ASSERT_EQ(*got, "first second");
}

TEST_F(curlTest, testDownloadAsStrDropsPartialContentOnFailure) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);
    // a cut transfer still wrote its head; returning it would look whole.
    EXPECT_CALL(*mock, easyPerform(aCurlHandle())).WillOnce([&mock](CURL*) {
        mock->pump("half a manifes");
        return CURLE_PARTIAL_FILE;
    });

    curl session;
    curl::res got = session.downloadAsStr("https://byeol.io/manifest");

    ASSERT_FALSE(got.has());
    ASSERT_EQ(got.getErr(), CURLE_PARTIAL_FILE);
}

TEST_F(curlTest, testGuardsTransferByStallNotByTotalTime) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);
    EXPECT_CALL(*mock, easyPerform(aCurlHandle())).WillOnce(Return(CURLE_OK));

    curl session;
    session.downloadAsStr("https://byeol.io/manifest");

    // a wall clock cap tight enough for a dead server would kill a slow link too.
    ASSERT_EQ(mock->opts.lowSpeedLimit, 1024);
    ASSERT_EQ(mock->opts.lowSpeedTime, 30);
    ASSERT_EQ(mock->opts.connectTimeout, 15);
}

TEST_F(curlTest, testSendsUserAgent) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);
    EXPECT_CALL(*mock, easyPerform(aCurlHandle())).WillOnce(Return(CURLE_OK));

    curl session;
    session.downloadAsStr("https://api.github.com/repos");

    // the github API answers 403 to a request that carries none.
    ASSERT_EQ(mock->opts.userAgent, "byeol-launcher");
}

TEST_F(curlTest, testFailsOnHttpErrorRatherThanStoringTheBody) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);
    EXPECT_CALL(*mock, easyPerform(aCurlHandle())).WillOnce(Return(CURLE_OK));

    curl session;
    session.downloadAsStr("https://byeol.io/missing");

    // without this the body of a 404 gets stored as if it were real content.
    ASSERT_EQ(mock->opts.failOnError, 1L);
}

TEST_F(curlTest, testFollowsRedirect) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);
    EXPECT_CALL(*mock, easyPerform(aCurlHandle())).WillOnce(Return(CURLE_OK));

    curl session;
    session.downloadAsStr("https://github.com/byeolang/byeol/releases/latest");

    // a github release redirects to another host to serve the real file.
    ASSERT_EQ(mock->opts.followLocation, 1L);
}

TEST_F(curlTest, testKeepsTimeoutsOffSignals) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);
    EXPECT_CALL(*mock, easyPerform(aCurlHandle())).WillOnce(Return(CURLE_OK));

    curl session;
    session.downloadAsStr("https://byeol.io/manifest");

    // signal based timeouts aren't safe to call from a thread.
    ASSERT_EQ(mock->opts.noSignal, 1L);
}

TEST_F(curlTest, testSendsGivenUrl) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);
    EXPECT_CALL(*mock, easyPerform(aCurlHandle())).WillOnce(Return(CURLE_OK));

    curl session;
    session.downloadAsStr("https://byeol.io/manifest.stela");

    ASSERT_EQ(mock->opts.url, "https://byeol.io/manifest.stela");
}

TEST_F(curlTest, testDownloadAsFileGivesBackItsPath) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);
    EXPECT_CALL(*mock, easyPerform(aCurlHandle())).WillOnce([&mock](CURL*) {
        mock->pump("toolchain bytes");
        return CURLE_OK;
    });

    filesystem::path path = _aTempPath("curlTestOk.zip");

    curl session;
    curl::res got = session.downloadAsFile("https://byeol.io/toolchain.zip", path.string());

    ASSERT_TRUE(got.has());
    ASSERT_EQ(*got, path.string());

    error_code ec;
    filesystem::remove(path, ec);
}

TEST_F(curlTest, testDownloadAsFileWritesReceivedContent) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);
    EXPECT_CALL(*mock, easyPerform(aCurlHandle())).WillOnce([&mock](CURL*) {
        mock->pump("toolchain bytes");
        return CURLE_OK;
    });

    filesystem::path path = _aTempPath("curlTestContent.zip");

    curl session;
    session.downloadAsFile("https://byeol.io/toolchain.zip", path.string());

    ifstream in(path, ios::binary);
    string wrote((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    ASSERT_EQ(wrote, "toolchain bytes");

    in.close();
    error_code ec;
    filesystem::remove(path, ec);
}

TEST_F(curlTest, testDownloadAsFileRemovesPartialFileOnFailure) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);
    EXPECT_CALL(*mock, easyPerform(aCurlHandle())).WillOnce([&mock](CURL*) {
        mock->pump("half a zip");
        return CURLE_PARTIAL_FILE;
    });

    filesystem::path path = _aTempPath("curlTestPartial.zip");

    curl session;
    curl::res got = session.downloadAsFile("https://byeol.io/toolchain.zip", path.string());

    ASSERT_FALSE(got.has());
    ASSERT_EQ(got.getErr(), CURLE_PARTIAL_FILE);
    // leaving it behind makes the next run mistake it for a complete file.
    ASSERT_FALSE(filesystem::exists(path));
}

TEST_F(curlTest, testDownloadAsFileFailsOnUnopenablePath) {
    scopedCurlMock mock;
    _expectHandleLifecycle(mock);
    // the path is refused before anything reaches the wire.
    EXPECT_CALL(*mock, easyPerform(_)).Times(0);

    curl session;
    curl::res got = session.downloadAsFile("https://byeol.io/toolchain.zip", "/no/such/dir/toolchain.zip");

    ASSERT_FALSE(got.has());
    ASSERT_EQ(got.getErr(), CURLE_WRITE_ERROR);
}

TEST_F(curlTest, testDownloadAsFileFailsWithoutHandle) {
    scopedCurlMock mock;
    EXPECT_CALL(*mock, easyInit()).WillOnce(Return(nullptr));
    EXPECT_CALL(*mock, easyPerform(_)).Times(0);

    filesystem::path path = _aTempPath("curlTestNoHandle.zip");

    curl session;
    curl::res got = session.downloadAsFile("https://byeol.io/toolchain.zip", path.string());

    ASSERT_FALSE(got.has());
    ASSERT_EQ(got.getErr(), CURLE_FAILED_INIT);
    // it gave up before opening anything, so nothing should be left on disk.
    ASSERT_FALSE(filesystem::exists(path));
}
