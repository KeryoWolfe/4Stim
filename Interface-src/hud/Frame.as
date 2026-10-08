package hud
{
	import flash.display.Shape;
	import flash.display.Sprite;

	// The L separator and the corner disc. The disc is a separate layer
	// (disc) for the panel to put above the navigation list, so the list's
	// corner tucks behind it like the separator's does. Drawn relative to the HUD's
	// anchor corner: (0, 0) is the screen corner, y goes up as it gets more
	// negative, and x is flipped when the HUD sits on the right.
	public class Frame extends Sprite
	{
		public static const DISC_R:Number = 150;
		public static const RING:Number = 6;

		private var _rules:Shape = new Shape();
		private var _disc:Shape = new Shape();

		public function Frame()
		{
			super();
			mouseEnabled = false;
			addChild(_rules);
		}

		public function get disc():Shape { return _disc; }

		// a_vRule / a_hRule: { x, y, w, h } in anchor space (already mirrored).
		public function draw(a_theme:Theme, a_mirror:Boolean, a_vRule:Object, a_hRule:Object):void
		{
			var g:* = _rules.graphics;
			g.clear();
			Theme.fill(g, a_theme.colors("separator"), a_vRule.x, a_vRule.y, a_vRule.w, a_vRule.h, Math.PI / 2, 1, 2.5);
			Theme.fill(g, a_theme.colors("separator"), a_hRule.x, a_hRule.y, a_hRule.w, a_hRule.h, a_mirror ? Math.PI : 0, 1, 2.5);

			// A full circle centered on the corner; only its quarter is on screen.
			var d:* = _disc.graphics;
			d.clear();
			Theme.begin(d, a_theme.colors("ring"), -DISC_R, -DISC_R, DISC_R * 2, DISC_R * 2, a_mirror ? -Math.PI / 4 : -Math.PI / 4 * 3);
			d.drawCircle(0, 0, DISC_R);
			d.endFill();
			d.beginFill(a_theme.color("disc"), a_theme.alpha("disc"));
			d.drawCircle(0, 0, DISC_R - RING);
			d.endFill();
			_disc.visible = a_theme.show("logo");
		}
	}
}
