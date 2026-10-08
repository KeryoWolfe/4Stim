package
{
	import flash.display.Graphics;
	import flash.display.Sprite;

	// Base for the default tab icons: white line art on a 64x64 stage, drawn
	// in the same 24x24 grid as the HUD mockup's icons. The HUD tints them
	// with the theme's colors, so they're drawn in plain white.
	public class TabIcon extends Sprite
	{
		protected static const SCALE:Number = 64 / 24;
		protected static const STROKE:Number = 2.2 * 64 / 24;

		protected function get g():Graphics { return graphics; }

		protected function pen():void
		{
			graphics.lineStyle(STROKE, 0xFFFFFF, 1, false, "normal", "round", "round");
		}

		protected function line(a_x1:Number, a_y1:Number, a_x2:Number, a_y2:Number):void
		{
			graphics.moveTo(a_x1 * SCALE, a_y1 * SCALE);
			graphics.lineTo(a_x2 * SCALE, a_y2 * SCALE);
		}

		protected function circle(a_x:Number, a_y:Number, a_r:Number):void
		{
			graphics.drawCircle(a_x * SCALE, a_y * SCALE, a_r * SCALE);
		}

		protected function dot(a_x:Number, a_y:Number, a_r:Number):void
		{
			graphics.lineStyle();
			graphics.beginFill(0xFFFFFF, 1);
			graphics.drawCircle(a_x * SCALE, a_y * SCALE, a_r * SCALE);
			graphics.endFill();
			pen();
		}

		// An arc around (a_x, a_y), from angle a_from to a_to (degrees,
		// clockwise from +x as on screen), as short line segments.
		protected function arc(a_x:Number, a_y:Number, a_r:Number, a_from:Number, a_to:Number):void
		{
			var steps:int = 24;
			for (var i:int = 0; i <= steps; i++) {
				var a:Number = (a_from + (a_to - a_from) * i / steps) * Math.PI / 180;
				var px:Number = (a_x + Math.cos(a) * a_r) * SCALE;
				var py:Number = (a_y + Math.sin(a) * a_r) * SCALE;
				if (i == 0) {
					graphics.moveTo(px, py);
				} else {
					graphics.lineTo(px, py);
				}
			}
		}
	}
}
