pragma Singleton
import QtQuick

// Mock-backed presentation contract каталога планов. Сортировка не является
// обязанностью страницы и позднее переедет в Avm_Plans/proxy model.
QtObject {
    readonly property var plans: MockCatalog.plans

    function sortedPlans(sortIndex) {
        const items = plans.slice();

        if (sortIndex === 0) {
            items.sort(function (a, b) {
                return b.ratingAvg - a.ratingAvg;
            });
        } else if (sortIndex === 1) {
            items.sort(function (a, b) {
                return b.reviewCount - a.reviewCount;
            });
        } else {
            items.sort(function (a, b) {
                return a.createdAt < b.createdAt ? 1 : -1;
            });
        }

        return items;
    }
}
