pragma Singleton
import QtQuick

// Асинхронный mock auth/users API. Методы и requestFinished повторяют форму
// будущего QObject-сервиса: вызов возвращает requestId, результат приходит
// отдельным сигналом. failMode: "none", "<key>" для всех операций либо
// "<operation>:<key>" (например, "login:incorrect_password").
QtObject {
    id: api

    property string failMode: "none"
    property bool failOnce: false
    property int responseDelayMs: 180
    readonly property bool busy: pendingRequests.length > 0 || responseTimer.running
    readonly property int pendingCount: pendingRequests.length + (responseTimer.running ? 1 : 0)

    property int nextRequestId: 1
    property int nextUserId: 2
    property int nextTokenId: 1
    property int nextTokenFamilyId: 1
    property var pendingRequests: []
    property var activeRequest: null
    property var accessTokenOwners: ({})
    property var refreshSessions: ({})
    property var retiredRefreshFamilies: ({})
    property var revokedTokenFamilies: ({})
    property var usersByEmail: ({
            "igor@example.com": {
                password: "Strong1!",
                user: {
                    id: "2e8cbeec-3528-45bc-908b-cdf76944d9c3",
                    email: "igor@example.com",
                    name: qsTr("Игорь Морозов"),
                    createdAt: "2026-01-12T09:30:00Z"
                },
                profile: {
                    age: 27,
                    gender: "male",
                    heightCm: 182,
                    weightKg: 78
                }
            }
        })

    signal requestStarted(int requestId, string operation)
    signal requestFinished(int requestId, string operation, bool ok, var data, var error)

    property Timer responseTimer: Timer {
        interval: Math.max(0, api.responseDelayMs)
        repeat: false

        onTriggered: api.finishActiveRequest()
    }

    function checkEmail(email) {
        return enqueue("checkEmail", {
            email: String(email).trim().toLowerCase()
        });
    }

    function signup(request) {
        return enqueue("signup", request === undefined || request === null ? {} : request);
    }

    function login(email, password) {
        return enqueue("login", {
            email: String(email).trim().toLowerCase(),
            password: String(password)
        });
    }

    function refresh(refreshToken) {
        return enqueue("refresh", {
            refreshToken: String(refreshToken)
        });
    }

    function getUser(userId, withProfile) {
        return enqueue("getUser", {
            userId: String(userId),
            withProfile: withProfile === undefined ? false : Boolean(withProfile)
        });
    }

    function logout() {
        MockSession.logout();
    }

    function reset() {
        responseTimer.stop();
        api.pendingRequests = [];
        api.activeRequest = null;
        api.nextRequestId = 1;
        api.nextUserId = 2;
        api.nextTokenId = 1;
        api.nextTokenFamilyId = 1;
        api.accessTokenOwners = {};
        api.refreshSessions = {};
        api.retiredRefreshFamilies = {};
        api.revokedTokenFamilies = {};
        api.failMode = "none";
        api.failOnce = false;
        api.usersByEmail = initialUsers();
        MockSession.reset();
    }

    function enqueue(operation, payload) {
        const request_id = api.nextRequestId++;

        api.pendingRequests = api.pendingRequests.concat([
            {
                requestId: request_id,
                operation: operation,
                payload: payload
            }
        ]);
        startNextRequest();
        return request_id;
    }

    function startNextRequest() {
        if (api.activeRequest !== null || api.pendingRequests.length === 0) {
            return;
        }

        api.activeRequest = api.pendingRequests[0];
        api.pendingRequests = api.pendingRequests.slice(1);
        api.requestStarted(api.activeRequest.requestId, api.activeRequest.operation);
        responseTimer.restart();
    }

    function finishActiveRequest() {
        const request = api.activeRequest;
        let result;

        if (request === null) {
            return;
        }

        api.activeRequest = null;
        result = forcedFailure(request.operation);

        if (result === null) {
            result = dispatch(request.operation, request.payload);
        }

        api.requestFinished(request.requestId, request.operation, result.ok, result.data, result.error);
        startNextRequest();
    }

    function dispatch(operation, payload) {
        switch (operation) {
        case "checkEmail":
            return handleCheckEmail(payload);
        case "signup":
            return handleSignup(payload);
        case "login":
            return handleLogin(payload);
        case "refresh":
            return handleRefresh(payload);
        case "getUser":
            return handleGetUser(payload);
        default:
            return failure("undefined_error", 500, "Unknown mock operation");
        }
    }

    function handleCheckEmail(payload) {
        const email = payload.email;

        if (!isValidEmail(email)) {
            return failure("undefined_error", 400, "Email validation failed");
        }

        return success({
            free: api.usersByEmail[email] === undefined
        });
    }

    function handleSignup(payload) {
        const email = String(payload.email === undefined ? "" : payload.email).trim().toLowerCase();
        const name = String(payload.name === undefined ? "" : payload.name).trim();
        const password = String(payload.password === undefined ? "" : payload.password);
        const password_error = passwordError(password);
        let record;
        let tokens;
        let user_id;

        if (!isValidEmail(email)) {
            return failure("invalid_email", 400, "Email is invalid");
        }
        if (api.usersByEmail[email] !== undefined) {
            return failure("email_taken", 400, "Email is already registered");
        }
        if (name.length < 2 || name.length > 80) {
            return failure("invalid_name", 400, "Name length must be between 2 and 80");
        }
        if (password_error.length > 0) {
            return failure(password_error, 400, "Password does not meet requirements");
        }

        user_id = makeUserId(api.nextUserId++);
        record = {
            password: password,
            user: {
                id: user_id,
                email: email,
                name: name,
                createdAt: new Date().toISOString()
            },
            profile: null
        };
        api.usersByEmail[email] = record;
        tokens = issueTokenPair(user_id);
        MockSession.authenticate(record.user, null, tokens);

        return success({
            user: record.user,
            tokens: tokens
        });
    }

    function handleLogin(payload) {
        const email = payload.email;
        const record = api.usersByEmail[email];
        let tokens;

        if (!isValidEmail(email)) {
            return failure("invalid_email", 400, "Email is invalid");
        }
        if (record === undefined) {
            return failure("undefined_error", 500, "User lookup failed");
        }
        if (payload.password !== record.password) {
            return failure("incorrect_password", 400, "Password is incorrect");
        }

        tokens = issueTokenPair(record.user.id);
        MockSession.authenticate(record.user, null, tokens);
        return success({
            user: record.user,
            tokens: tokens
        });
    }

    function handleRefresh(payload) {
        const refresh_token = payload.refreshToken;
        const refresh_session = api.refreshSessions[refresh_token];
        const retired_family = api.retiredRefreshFamilies[refresh_token];
        let tokens;

        if (retired_family !== undefined) {
            revokeTokenFamily(retired_family);
            MockSession.expire();
            return failure("invalid_token", 401, "Refresh token was already used");
        }
        if (refresh_token.length === 0 || refresh_session === undefined || api.revokedTokenFamilies[refresh_session.familyId]) {
            MockSession.expire();
            return failure("invalid_token", 401, "Refresh token is invalid");
        }

        delete api.refreshSessions[refresh_token];
        api.retiredRefreshFamilies[refresh_token] = refresh_session.familyId;
        delete api.accessTokenOwners[refresh_session.accessToken];
        tokens = issueTokenPair(refresh_session.userId, refresh_session.familyId);
        MockTokenStore.save(tokens);
        return success({
            tokens: tokens
        });
    }

    function handleGetUser(payload) {
        const record = findUserById(payload.userId);
        const token_session = api.accessTokenOwners[MockTokenStore.accessToken];

        if (!MockTokenStore.hasTokens || token_session === undefined || api.revokedTokenFamilies[token_session.familyId]) {
            return failure("invalid_token", 401, "Access token is missing");
        }
        if (record === null) {
            return failure("not_found", 404, "User was not found");
        }

        return success(userResponse(record, payload.withProfile));
    }

    function forcedFailure(operation) {
        const mode = api.failMode.trim();
        const separator = mode.indexOf(":");
        let key = mode;

        if (mode.length === 0 || mode === "none") {
            return null;
        }
        if (separator >= 0) {
            if (mode.substring(0, separator) !== operation) {
                return null;
            }
            key = mode.substring(separator + 1);
        }
        if (api.failOnce) {
            api.failMode = "none";
            api.failOnce = false;
        }
        if (operation === "refresh" && (key === "invalid_token" || key === "token_expired")) {
            MockSession.expire();
        }

        return failure(key.length > 0 ? key : "undefined_error", statusForKey(key), "Forced mock failure");
    }

    function statusForKey(key) {
        if (key === "email_taken") {
            return 400;
        }
        if (key === "email_not_found" || key === "not_found") {
            return 404;
        }
        if (key === "invalid_token" || key === "token_expired") {
            return 401;
        }
        if (key === "rate_limited") {
            return 429;
        }
        if (key === "server_error" || key === "undefined_error") {
            return 500;
        }
        if (key === "network_error" || key === "timeout") {
            return 0;
        }
        return 400;
    }

    function success(data) {
        return {
            ok: true,
            data: data,
            error: null
        };
    }

    function failure(key, httpStatus, message) {
        return {
            ok: false,
            data: null,
            error: {
                key: key.length > 0 ? key : "undefined_error",
                message: message,
                httpStatus: httpStatus
            }
        };
    }

    function isValidEmail(email) {
        return /^[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}$/i.test(email);
    }

    function passwordError(password) {
        if (password.length < 8) {
            return "password_too_short";
        }
        if (password.length > 64) {
            return "password_too_long";
        }
        if (!/^[\x21-\x7E]+$/.test(password)) {
            return "invalid_password";
        }
        if (!/[A-Za-z]/.test(password)) {
            return "password_missing_letter";
        }
        if (!/[0-9]/.test(password)) {
            return "password_missing_digit";
        }
        if (!/[^A-Za-z0-9]/.test(password)) {
            return "password_missing_special";
        }
        return "";
    }

    function makeUserId(sequence) {
        return "00000000-0000-4000-8000-" + String(sequence).padStart(12, "0");
    }

    function userResponse(record, withProfile) {
        const response = {
            id: record.user.id,
            name: record.user.name,
            email: record.user.email,
            createdAt: record.user.createdAt
        };

        if (withProfile && record.profile !== null) {
            response.profile = record.profile;
        }
        return response;
    }

    function issueTokenPair(userId, existingFamilyId) {
        const token_id = api.nextTokenId++;
        const family_id = existingFamilyId === undefined ? "family-" + api.nextTokenFamilyId++ : existingFamilyId;
        const access_token = "mock-access-" + token_id;
        const refresh_token = "mock-refresh-" + token_id;
        const tokens = {
            accessToken: access_token,
            refreshToken: refresh_token
        };

        api.accessTokenOwners[access_token] = {
            userId: userId,
            familyId: family_id
        };
        api.refreshSessions[refresh_token] = {
            userId: userId,
            accessToken: access_token,
            familyId: family_id
        };
        return tokens;
    }

    function revokeTokenFamily(familyId) {
        const access_tokens = Object.keys(api.accessTokenOwners);
        const refresh_tokens = Object.keys(api.refreshSessions);

        api.revokedTokenFamilies[familyId] = true;
        for (let i = 0; i < access_tokens.length; ++i) {
            if (api.accessTokenOwners[access_tokens[i]].familyId === familyId) {
                delete api.accessTokenOwners[access_tokens[i]];
            }
        }
        for (let j = 0; j < refresh_tokens.length; ++j) {
            if (api.refreshSessions[refresh_tokens[j]].familyId === familyId) {
                delete api.refreshSessions[refresh_tokens[j]];
            }
        }
    }

    function findUserById(userId) {
        const emails = Object.keys(api.usersByEmail);

        for (let i = 0; i < emails.length; ++i) {
            const record = api.usersByEmail[emails[i]];

            if (record.user.id === userId) {
                return record;
            }
        }
        return null;
    }

    function initialUsers() {
        return {
            "igor@example.com": {
                password: "Strong1!",
                user: {
                    id: "2e8cbeec-3528-45bc-908b-cdf76944d9c3",
                    email: "igor@example.com",
                    name: qsTr("Игорь Морозов"),
                    createdAt: "2026-01-12T09:30:00Z"
                },
                profile: {
                    age: 27,
                    gender: "male",
                    heightCm: 182,
                    weightKg: 78
                }
            }
        };
    }
}
