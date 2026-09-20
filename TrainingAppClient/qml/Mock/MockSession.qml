pragma Singleton
import QtQuick

// Auth-сессия, которую будет читать роутинг issue 2. Состояния совпадают с
// будущей Avm_Auth: anonymous / authenticated / sessionExpired.
QtObject {
    id: session

    readonly property bool authenticated: state === "authenticated" && user !== null && MockTokenStore.hasTokens
    property var localProfile: null
    property bool offline: false
    property var profile: null
    property string state: "anonymous"
    property var user: null
    readonly property string userId: user !== null && user.id ? String(user.id) : ""

    signal authenticatedChangedByApi
    signal loggedOut
    signal sessionExpired

    function authenticate(userDto, profileDto, tokensDto) {
        if (userDto === undefined || userDto === null || !userDto.id || !MockTokenStore.save(tokensDto)) {
            console.warn("MockSession: неполные данные авторизации");
            return false;
        }

        session.user = userDto;
        session.profile = profileDto === undefined ? null : profileDto;
        session.offline = false;
        session.state = "authenticated";
        session.authenticatedChangedByApi();
        return true;
    }
    function clearLocalProfile() {
        session.localProfile = null;
    }
    function expire() {
        MockTokenStore.clear();
        session.user = null;
        session.profile = null;
        session.localProfile = null;
        session.offline = false;
        session.state = "sessionExpired";
        session.sessionExpired();
    }
    function logout() {
        MockTokenStore.clear();
        session.user = null;
        session.profile = null;
        session.localProfile = null;
        session.offline = false;
        session.state = "anonymous";
        session.loggedOut();
    }
    function reset() {
        MockTokenStore.clear();
        session.user = null;
        session.profile = null;
        session.localProfile = null;
        session.offline = false;
        session.state = "anonymous";
    }
    function restore(userDto, profileDto, isOffline) {
        if (!MockTokenStore.hasTokens || userDto === undefined || userDto === null || !userDto.id) {
            return false;
        }

        session.user = userDto;
        session.profile = profileDto === undefined ? null : profileDto;
        session.offline = Boolean(isOffline);
        session.state = "authenticated";
        session.authenticatedChangedByApi();
        return true;
    }
    function saveLocalProfile(localProfileDto) {
        if (localProfileDto === undefined || localProfileDto === null) {
            return false;
        }

        session.localProfile = localProfileDto;
        return true;
    }
}
