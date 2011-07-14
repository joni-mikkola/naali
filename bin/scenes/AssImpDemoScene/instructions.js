var showUi = true;
if (server.IsRunning() && framework.IsHeadless())
    showUi = false;

if (showUi)
{
    engine.ImportExtension("qt.core");
    engine.ImportExtension("qt.gui");

    var label = new QLabel();
    label.indent = 0;
    label.text = "Collada importer demo scene";
    label.resize(300,200);
    label.setStyleSheet("QLabel {background-color: transparent; font-size: 16px; }");

    var proxy = new UiProxyWidget(label);
    ui.AddProxyWidgetToScene(proxy);
    proxy.x = 50
    proxy.y = 0;
    proxy.windowFlags = 0;
    proxy.visible = true;
}
