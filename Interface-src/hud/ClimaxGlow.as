package hud
{
	import flash.display.GradientType;
	import flash.display.Shape;
	import flash.events.Event;
	import flash.geom.Matrix;
	import flash.utils.getTimer;

	// The climax flash: a faint white glow along the screen's edges only
	// (nothing in the middle), that rises, holds for a moment and fades out.
	// Drawn over the whole visible screen, behind the HUD panels.
	public class ClimaxGlow extends Shape
	{
		private static const RISE:Number = 180;   // ms
		private static const HOLD:Number = 200;
		private static const FALL:Number = 1200;

		private var _start:int = -1;
		private var _strength:Number = 1;

		public function ClimaxGlow()
		{
			super();
			visible = false;
		}

		public function play(a_strength:Number, a_screen:Object):void
		{
			_strength = Math.max(0, Math.min(1, a_strength));
			if (_strength <= 0) {
				return;
			}
			draw(a_screen);
			_start = getTimer();
			alpha = 0;
			visible = true;
			addEventListener(Event.ENTER_FRAME, onFrame);
		}

		private function draw(a_screen:Object):void
		{
			var x:Number = a_screen.x1;
			var y:Number = a_screen.y1;
			var w:Number = a_screen.x2 - a_screen.x1;
			var h:Number = a_screen.y2 - a_screen.y1;
			var band:Number = Math.round(Math.min(w, h) * 0.07);  // how far in from each edge
			graphics.clear();
			// A thin glow along each edge, gone well before the middle.
			edge(x, y, w, band, Math.PI / 2, false);         // top: fades downward
			edge(x, y + h - band, w, band, -Math.PI / 2, false);  // bottom: fades upward
			edge(x, y, band, h, 0, true);                     // left: fades rightward
			edge(x + w - band, y, band, h, Math.PI, true);    // right: fades leftward
		}

		private function edge(a_x:Number, a_y:Number, a_w:Number, a_h:Number, a_angle:Number, a_vertical:Boolean):void
		{
			var m:Matrix = new Matrix();
			m.createGradientBox(a_w, a_h, a_angle, a_x, a_y);
			graphics.beginGradientFill(GradientType.LINEAR, [0xFFFFFF, 0xFFFFFF], [0.14, 0], [0, 255], m);
			graphics.drawRect(a_x, a_y, a_w, a_h);
			graphics.endFill();
		}

		private function onFrame(a_event:Event):void
		{
			var t:Number = getTimer() - _start;
			var a:Number;
			if (t < RISE) {
				a = t / RISE;
			} else if (t < RISE + HOLD) {
				a = 1;
			} else if (t < RISE + HOLD + FALL) {
				var f:Number = (t - RISE - HOLD) / FALL;
				a = 1 - f * f * (3 - 2 * f);  // smooth fade
			} else {
				a = 0;
				visible = false;
				removeEventListener(Event.ENTER_FRAME, onFrame);
			}
			alpha = a * _strength;
		}
	}
}
