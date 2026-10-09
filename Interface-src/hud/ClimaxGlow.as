package hud
{
	import flash.display.GradientType;
	import flash.display.Shape;
	import flash.events.Event;
	import flash.geom.Matrix;
	import flash.utils.getTimer;

	// The climax flash: a soft white glow that rises at the screen's edges
	// (and a faint wash over the middle), holds for a moment and fades out.
	// Drawn over the whole visible screen, behind the HUD panels.
	public class ClimaxGlow extends Shape
	{
		private static const RISE:Number = 120;   // ms
		private static const HOLD:Number = 350;
		private static const FALL:Number = 1400;

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
			graphics.clear();
			// The edges: transparent in the middle, white toward the corners.
			var m:Matrix = new Matrix();
			m.createGradientBox(w * 1.25, h * 1.25, 0, x - w * 0.125, y - h * 0.125);
			graphics.beginGradientFill(GradientType.RADIAL, [0xFFFFFF, 0xFFFFFF, 0xFFFFFF], [0.12, 0.22, 0.75], [0, 150, 255], m);
			graphics.drawRect(x, y, w, h);
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
