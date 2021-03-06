import QtQuick 2.10
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import de.skycoder42.syrem 1.0

Item {
	id: snoozeView

	property var times
	onTimesChanged: {
		if(!inputValue)
			elementList.model = times;
	}

	property var inputValue
	onInputValueChanged:  {
		if(inputValue) {
			if(typeof inputValue.toList !== "undefined")
				elementList.model = inputValue.toList();
			else
				elementList.model = inputValue;
		}
	}

	implicitHeight: layout.implicitHeight

	ColumnLayout {
		id: layout

		anchors.fill: parent

		ScrollView {
			id: scrollView
			implicitHeight: dummyDelegate.height * (elementList.model ? elementList.model.length : 1)

			Layout.minimumHeight: dummyDelegate.height
			Layout.preferredHeight: implicitHeight
			Layout.fillWidth: true
			Layout.fillHeight: true
			clip: true

			ListView {
				id: elementList

				ItemDelegate {
					id: dummyDelegate
					visible: false
					text: "dummy"
				}

				delegate: ItemDelegate {
					width: scrollView.width
					text: editInput.visible ? "" : modelData

					onClicked: {
						editInput.visible = true;
						editInput.activate();
					}

					SnoozeEdit {
						id: editInput
						anchors.left: parent.left
						anchors.leftMargin: 16
						anchors.right: parent.right
						anchors.verticalCenter: parent.verticalCenter
						visible: false

						text: modelData
						edit: true

						onFocusLost: visible = false

						onEditDone: {
							visible = false;
							var nList = elementList.model;
							nList[index] = editInput.text;
							inputValue = SnoozeTimesGenerator.generate(nList);
						}

						onRemoved: {
							visible = false;
							var nList = elementList.model;
							nList.splice(index, 1);
							inputValue = SnoozeTimesGenerator.generate(nList);
						}
					}
				}
			}
		}

		SnoozeEdit {
			id: addInput
			Layout.minimumHeight: implicitHeight
			Layout.maximumHeight: implicitHeight
			Layout.fillWidth: true

			onEditDone: {
				var nList = elementList.model;
				nList.push(text);
				inputValue = SnoozeTimesGenerator.generate(nList);
				addInput.text = "";
			}
		}
	}
}
