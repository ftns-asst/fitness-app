pragma Singleton
import QtQuick

QtObject {
    function has(value) {
        return value !== null && value !== undefined && isFinite(Number(value));
    }
    function number(value, digits = 1) {
        return has(value) ? Qt.locale().toString(Number(value), "f", digits) : "—";
    }
    function signed(value, digits = 1) {
        return has(value) ? (value > 0 ? "+" : value < 0 ? "−" : "") + number(Math.abs(value), digits) : "—";
    }
    function fullDate(iso) {
        const parsed = new Date(String(iso).slice(0, 10) + "T12:00:00");
        return isNaN(parsed.getTime()) ? "" : parsed.toLocaleDateString(Qt.locale(), "d MMM yyyy");
    }
    function setsText(value) {
        const count = Number(value), last = count % 10, hundred = count % 100;
        const word = hundred >= 11 && hundred <= 14 ? qsTr("подходов") : last === 1 ? qsTr("подход") : last >= 2 && last <= 4 ? qsTr("подхода") : qsTr("подходов");
        return count + " " + word;
    }
    function date(iso) {
        const parsed = new Date(String(iso).slice(0, 10) + "T12:00:00");
        return isNaN(parsed.getTime()) ? "" : parsed.toLocaleDateString(Qt.locale(), "d MMM");
    }
    function rows(model, revision = 0) {
        if (!model)
            return [];
        if (typeof model.get !== "function") {
            const items = [];
            for (let i = 0; i < (model.length || 0); ++i)
                items.push(model[i]);
            return items;
        }
        const count = model.count !== undefined ? model.count : model.rowCount();
        const result = [];
        for (let i = 0; i < count; ++i)
            result.push(model.get(i));
        return result;
    }
    function period(referenceDate, index) {
        if (index === 2)
            return {
                from: "",
                to: ""
            };
        const end = new Date(referenceDate + "T12:00:00");
        if (isNaN(end.getTime()))
            return {
                from: "",
                to: ""
            };
        const start = new Date(end.getTime());
        start.setDate(start.getDate() - (index === 0 ? 6 : 27));
        return {
            from: Qt.formatDate(start, Qt.ISODate),
            to: Qt.formatDate(end, Qt.ISODate)
        };
    }
    function points(model, key, averageKey = "", revision = 0) {
        return rows(model).filter(row => has(row[key])).map(row => ({
                    date: row.date,
                    value: Number(row[key]),
                    average: averageKey && has(row[averageKey]) ? Number(row[averageKey]) : null
                }));
    }
}
