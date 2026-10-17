package
{
	import flash.display.MovieClip;
	import flash.display.Shape;
	import flash.events.Event;
	import flash.geom.ColorTransform;
	import flash.utils.getTimer;

	// The default logo movie for the 4Stim HUD (Data\Interface\4Stim\HUDLogo.swf):
	// the 4Stim logo, turning slowly. A logo movie can be anything that
	// animates by itself on a 256x256 stage -- replace this file, or point a
	// theme's "logoMovie" at your own (HUD_API.md). The functions below are
	// optional extras the HUD calls if they exist.
	//
	// The logo itself is LogoArt.as (vector art, shared with the MCM page).
	[SWF(width="256", height="256", frameRate="60", backgroundColor="#000000")]
	public class HUDLogo extends MovieClip
	{
		private static const SIZE:Number = LogoArt.SIZE;
		private static const SECONDS_PER_TURN:Number = 12;

		private var _spinner:Shape = new Shape();  // drawn around its origin, turned around it
		private var _paused:Boolean = false;
		private var _angle:Number = 0;
		private var _last:int = 0;
		private var _speedFactor:Number = 1;

		public function HUDLogo()
		{
			super();
			drawLogo();
			_spinner.x = SIZE / 2;
			_spinner.y = SIZE / 2;
			addChild(_spinner);
			_last = getTimer();
			addEventListener(Event.ENTER_FRAME, onEnterFrame);
		}

		// Turn by elapsed time, so the speed is the same at any frame rate.
		private function onEnterFrame(a_event:Event):void
		{
			var now:int = getTimer();
			var dt:Number = (now - _last) / 1000;
			_last = now;
			if (_paused || dt <= 0 || dt > 0.5) {
				return;
			}
			_angle = (_angle + 360 * dt * _speedFactor / SECONDS_PER_TURN) % 360;
			_spinner.rotation = _angle;
		}

		private function drawLogo():void
		{
			LogoArt.draw(_spinner.graphics);
		}

		// ---- Optional HUD calls ----

		// Themes with a "tint" (like Pip-Boy) get a one-color logo; others
		// show its own gradient.
		public function setTheme(a_theme:Object):void
		{
			var ct:ColorTransform = new ColorTransform();
			if (a_theme != null && a_theme.tint != null) {
				var hex:String = String(a_theme.tint).replace("#", "");
				ct.color = uint(parseInt(hex.substr(0, 6), 16));
			}
			_spinner.transform.colorTransform = ct;
		}

		// Spins a little faster at higher scene speeds.
		public function onSpeedChanged(a_level:int, a_count:int):void
		{
			_speedFactor = a_count > 1 ? 1 + a_level / (a_count - 1) : 1;
		}

		public function setPaused(a_paused:Boolean):void
		{
			_paused = a_paused;
			_last = getTimer();
		}
	}
}
