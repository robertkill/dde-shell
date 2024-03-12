// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15

import org.deepin.ds 1.0
import org.deepin.ds.dock 1.0
import org.deepin.dtk 1.0 as D
import Qt.labs.platform 1.1 as LP

Item {
    id: root
    required property int displayMode
    required property int colorTheme
    required property bool active
    required property bool attention
    required property string itemId
    required property string name
    required property string iconName
    required property string menus
    required property list<string> windows
    required property int visualIndex

    signal clickItem(itemId: string, menuId: string)

    Drag.active: mouseArea.drag.active
    Drag.source: root
    Drag.hotSpot.x: icon.width / 2
    Drag.hotSpot.y: icon.height / 2
    Drag.dragType: Drag.Automatic
    Drag.mimeData: { "text/x-dde-dock-dnd-appid": itemId }

    property int statusIndicatorSize: root.width * 0.9
    property int iconSize: root.width * 0.6

    Item {
        anchors.fill: parent
        id: appItem
        visible: !root.Drag.active // When in dragging, hide app item
        AppItemPalette {
            id: itemPalette
            displayMode: root.displayMode
            colorTheme: root.colorTheme
            active: root.active
            backgroundColor: "orange"
        }

        StatusIndicator {
            id: statusIndicator
            palette: itemPalette
            width: root.statusIndicatorSize
            height: root.statusIndicatorSize
            anchors.centerIn: parent
            visible: root.displayMode === Dock.Efficient && root.windows.length > 0

            SequentialAnimation {
                loops: Animation.Infinite
                running: root.attention
                PropertyAnimation {
                    target: statusIndicator
                    property: "color"
                    from: statusIndicator.palette.rectIndicator
                    to: "#CCF18A2E"
                    duration: 1000
                    easing.type: Easing.OutCubic
                }
                PropertyAnimation {
                    target: statusIndicator
                    property: "color"
                    from: "#CCF18A2E"
                    to: statusIndicator.palette.rectIndicator
                    duration: 1000
                    easing.type: Easing.InCubic
                }
            }
        }

        WindowIndicator {
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                bottomMargin: 1
            }
            windows: root.windows
            displayMode: root.displayMode
            palette: itemPalette
            visible: (root.displayMode === Dock.Efficient && root.windows.length > 1) || (root.displayMode === Dock.Fashion && root.windows.length > 0)
        }

        LP.Menu {
            id: contextMenu
            Instantiator {
                id: menuItemInstantiator
                model: JSON.parse(menus)
                delegate: LP.MenuItem {
                    text: modelData.name
                    onTriggered: {
                        root.clickItem(root.itemId, modelData.id)
                    }
                }
                onObjectAdded: (index, object) => contextMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => contextMenu.removeItem(object)
            }
        }

        D.DciIcon {
            id: icon
            name: root.iconName
            sourceSize: Qt.size(iconSize, iconSize)
            anchors.centerIn: parent

            SequentialAnimation {
                id: beatAnimation
                loops: Animation.Infinite
                running: root.attention && root.displayMode === Dock.Fashion
                PropertyAnimation {
                    target: icon
                    property: "scale"
                    from: 1
                    to: 1.3
                    duration: 150
                }
                PropertyAnimation {
                    target: icon
                    property: "scale"
                    from: 1.3
                    to: 1
                    duration: 150
                }
                PropertyAnimation {
                    target: icon
                    property: "scale"
                    from: 1
                    to: 1.2
                    duration: 100
                }
                PropertyAnimation {
                    target: icon
                    property: "scale"
                    from: 1.2
                    to: 1
                    duration: 100
                }
                PropertyAnimation {
                    target: icon
                    property: "scale"
                    from: 1
                    to: 1.1
                    duration: 100
                }
                PropertyAnimation {
                    target: icon
                    property: "scale"
                    from: 1.1
                    to: 1
                    duration: 100
                }
                PropertyAnimation {
                    target: icon
                    property: "scale"
                    from: 1
                    to: 1
                    duration: 1000
                }
                onStopped: icon.scale = 1
                onFinished: loops = Animation.Infinite
            }
            SequentialAnimation {
                id: launchAnimation
                loops: 1
                running: false
                property int y: 0
                PropertyAnimation {
                    target: icon
                    property: "y"
                    from: icon.y
                    to: icon.y - height * 0.15
                    duration: 60
                }
                PropertyAnimation {
                    target: icon
                    property: "y"
                    from: icon.y - height * 0.15
                    to: icon.y - height * 0.05
                    duration: 60
                }
                PropertyAnimation {
                    target: icon
                    property: "y"
                    from: icon.y - height * 0.05
                    to: icon.y - height * 0.15
                    duration: 60
                }
                PropertyAnimation {
                    target: icon
                    property: "y"
                    from: icon.y - height * 0.15
                    to: icon.y - height * 0.10
                    duration: 60
                }
                PropertyAnimation {
                    target: icon
                    property: "y"
                    from: icon.y - height * 0.10
                    to: icon.y - height * 0.15
                    duration: 60
                }
                PropertyAnimation {
                    target: icon
                    property: "y"
                    from: icon.y - height * 0.15
                    to: icon.y - height * 0.15
                    duration: 500
                }
                PropertyAnimation {
                    target: icon
                    property: "y"
                    from: icon.y - height * 0.15
                    to: icon.y
                    duration: 60
                }
                onStarted: {
                    icon.anchors.centerIn = null
                }
                onStopped: {
                    icon.anchors.centerIn = appItem
                }
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        drag.target: root
        onPressed: function (mouse) {
            if (mouse.button === Qt.LeftButton) {
                icon.grabToImage(function(result) {
                    root.Drag.imageSource = result.url;
                })
            }
        }
        onClicked: function (mouse) {
            if (mouse.button === Qt.RightButton) {
                MenuHelper.openMenu(contextMenu)
            } else {
                if (root.windows.length === 0) {
                    launchAnimation.start()
                }
                root.clickItem(root.itemId, "")
            }
        }

        onEntered: {
            if (windows.length === 0) return
            var itemPos = root.mapToItem(null, 0, 0)
            if (Panel.position % 2 === 0) {
                itemPos.x += (root.iconSize / 2)
            } else {
                itemPos.y += (root.iconSize / 2)
            }
            taskmanager.Applet.showWindowsPreview(windows, Panel.rootObject, itemPos.x, itemPos.y, Panel.position)
        }

        onExited: {
            if (windows.length === 0) return
            taskmanager.Applet.hideWindowsPreview()
        }
    }
}
