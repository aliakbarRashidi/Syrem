import QtQuick 2.10
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import de.skycoder42.remindme 1.0

Item {
	id: snoozeView

	property var times
	onTimesChanged: {
		if(!inputValue)
			elementList.model = times;
	}

	property var inputValue //BUG not successfully returned to c++
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

		ListView {
			id: elementList

			implicitHeight: dummyDelegate.height * (model ? model.length : 1)

			Layout.minimumHeight: dummyDelegate.height
			Layout.preferredHeight: implicitHeight
			Layout.fillWidth: true
			Layout.fillHeight: true
			clip: true

			delegate: ItemDelegate {
				width: parent.width
				text: editInput.visible ? "" : modelData

				onClicked: editInput.visible = true

				SnoozeEdit {
					id: editInput
					anchors.centerIn: parent
					width: parent.width
					visible: false

					text: modelData
					edit: true

					onFocusChanged: {
						if(!focus)
							visible = false;
					}

					onEditDone: {
						visible = false;
						var nList = elementList.model;
						nList[index] = editInput.text;
						inputValue = SnoozeTimes.generate(nList);
					}

					onRemoved: {
						visible = false;
						var nList = elementList.model;
						nList.splice(index, 1);
						inputValue = SnoozeTimes.generate(nList);
					}
				}
			}

			ItemDelegate {
				id: dummyDelegate
				visible: false
				text: "dummy"
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
				inputValue = SnoozeTimes.generate(nList);
				addInput.text = "";
			}
		}
	}
}