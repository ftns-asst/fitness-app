import QtQuick
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase
    name: "NetworkBanner"
    QtObject {
        id: network
        property string status: "unknown"
    }
    Component {
        id: factory
        NetworkStatusBanner {
            monitor: network
            width: 360
        }
    }
    function test_bannerCanBeDismissedAndResetsOnReconnect() {
        network.status = "unknown";
        const banner = createTemporaryObject(factory, testCase);
        compare(banner.height, 0);
        network.status = "offline";
        verify(banner.height > 0);
        banner.dismiss();
        compare(banner.height, 0);
        network.status = "online";
        compare(banner.height, 0);
        network.status = "offline";
        verify(banner.height > 0);
    }
}
