const widgetName = "org.senemos.nabu.flashlight";

for (const panelId of panelIds) {
    const panel = panelById(panelId);
    if (!panel)
        continue;
    for (const widgetId of panel.widgetIds) {
        const appletWidget = panel.widgetById(widgetId);
        if (!appletWidget)
            continue;
        if (appletWidget.type !== "org.kde.plasma.systemtray")
            continue;
        const systemtrayId = appletWidget.readConfig("SystrayContainmentId", "");
        // Plasma 6 stores the tray containment inline on some layouts instead
        // of exposing SystrayContainmentId.  In that case the tray widget owns
        // the same General/extraItems configuration through the public API.
        const systray = systemtrayId ? desktopById(systemtrayId) : appletWidget;
        if (!systray)
            continue;
        systray.currentConfigGroup = ["General"];
        const extraItems = String(systray.readConfig("extraItems") || "")
            .split(",").filter(item => item.length > 0);
        if (!extraItems.includes(widgetName)) {
            extraItems.push(widgetName);
            systray.writeConfig("extraItems", extraItems);
            systray.reloadConfig();
        }
    }
}
