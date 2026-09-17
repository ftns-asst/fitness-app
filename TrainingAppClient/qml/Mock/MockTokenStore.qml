pragma Singleton
import QtQuick

// In-memory аналог будущего AsToken_Store. Персистентность намеренно не
// используется: mock-сессия должна начинаться с чистого состояния при запуске.
QtObject {
    id: store

    property string accessToken: ""
    property string refreshToken: ""
    readonly property bool hasTokens: accessToken.length > 0 && refreshToken.length > 0
    readonly property string authorizationHeader: accessToken.length > 0 ? "Bearer " + accessToken : ""

    signal tokensSaved
    signal tokensCleared

    function save(tokens) {
        if (tokens === undefined || tokens === null || !tokens.accessToken || !tokens.refreshToken) {
            console.warn("MockTokenStore: получен неполный DTO токенов");
            return false;
        }

        store.accessToken = String(tokens.accessToken);
        store.refreshToken = String(tokens.refreshToken);
        store.tokensSaved();
        return true;
    }

    function clear() {
        store.accessToken = "";
        store.refreshToken = "";
        store.tokensCleared();
    }

    function snapshot() {
        return {
            accessToken: store.accessToken,
            refreshToken: store.refreshToken
        };
    }
}
