import QtQuick
Canvas {
 id: icon
 property int kind: 0
 property color ink: "#72b9e8"
 implicitWidth: 42; implicitHeight: 42
 onInkChanged: requestPaint()
 onKindChanged: requestPaint()
 onPaint: {
  let c = getContext("2d")
  c.reset(); c.scale(width / 48, height / 48); c.strokeStyle = ink; c.lineWidth = 2.5; c.lineCap = "round"; c.lineJoin = "round"
  if(kind === 0) {
   c.strokeRect(8, 5, 32, 38); c.beginPath(); c.arc(24, 21, 10, 0, Math.PI*2); c.stroke()
   c.beginPath(); c.moveTo(24,21); c.lineTo(34,32); c.stroke(); c.fillStyle=ink; c.fillRect(14,37,4,2); c.fillRect(23,37,9,2)
  } else if(kind === 1) {
   c.beginPath();c.arc(24,24,17,0,Math.PI*2);c.stroke();c.moveTo(24,12);c.lineTo(24,32);c.moveTo(16,24);c.lineTo(24,32);c.lineTo(32,24);c.stroke()
  } else if(kind === 2) {
   c.strokeRect(8,8,32,32);c.beginPath();c.moveTo(18,5);c.lineTo(18,0);c.moveTo(30,5);c.lineTo(30,0);c.moveTo(18,43);c.lineTo(18,48);c.moveTo(30,43);c.lineTo(30,48);c.stroke()
   c.beginPath();c.moveTo(15,26);c.lineTo(21,32);c.lineTo(33,17);c.stroke()
  } else if(kind === 3) {
   c.strokeRect(15,18,18,25);c.strokeRect(18,5,12,13);c.fillStyle=ink;c.fillRect(21,8,2,5);c.fillRect(26,8,2,5)
  } else if(kind === 4) {
   c.beginPath();c.arc(20,20,13,0,Math.PI*2);c.moveTo(30,30);c.lineTo(42,42);c.stroke();c.moveTo(13,21);c.lineTo(18,26);c.lineTo(27,15);c.stroke()
  } else {
   c.strokeRect(11,5,26,38);c.beginPath();for(let y=15;y<=33;y+=9){c.moveTo(17,y);c.lineTo(31,y)}c.stroke()
  }
 }
}
