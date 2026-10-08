package hud
{
	import flash.display.Shape;
	import flash.display.Sprite;
	import flash.text.TextField;
	import flash.text.TextFormat;
	import flash.text.TextFormatAlign;

	// The actor name bars and, beside them, the vertical speed meter.
	// Actors are { role, name, sex, isPlayer, meter } (HUD_API.md); meter -1
	// means nothing fills the bars yet, so only the track shows.
	public class Meters extends Sprite
	{
		private static const BAR_W:Number = 220;
		private static const BAR_H:Number = 7;
		private static const NAME_H:Number = 19;  // name line + gap above the bar
		private static const ROW_GAP:Number = 6;
		private static const SPEED_GAP:Number = 20;
		private static const SPEED_W:Number = 9;

		private var _theme:Theme;
		private var _mirror:Boolean = false;
		private var _actors:Array = [];
		private var _speed:int = 0;
		private var _speedCount:int = 0;

		private var _names:Sprite = new Sprite();
		private var _speedMeter:Sprite = new Sprite();

		public function Meters()
		{
			super();
			mouseEnabled = false;
			mouseChildren = false;
			addChild(_names);
			addChild(_speedMeter);
		}

		public function setTheme(a_theme:Theme, a_mirror:Boolean):void
		{
			_theme = a_theme;
			_mirror = a_mirror;
			redraw();
		}

		public function setActors(a_actors:Array):void
		{
			_actors = a_actors != null ? a_actors : [];
			redraw();
		}

		public function setMeters(a_values:Array):void
		{
			for (var i:int = 0; i < _actors.length && i < a_values.length; i++) {
				_actors[i].meter = Number(a_values[i]);
			}
			redraw();
		}

		public function setSpeed(a_level:int, a_count:int):void
		{
			_speed = a_level;
			_speedCount = a_count;
			redraw();
		}

		private function rowsHeight():Number
		{
			var rows:int = Math.max(1, _actors.length);
			return rows * (NAME_H + BAR_H) + (rows - 1) * ROW_GAP;
		}

		private function redraw():void
		{
			if (_theme == null) {
				return;
			}
			while (_names.numChildren > 0) {
				_names.removeChildAt(0);
			}
			_names.visible = _theme.show("actorMeters");
			_speedMeter.visible = _theme.show("speedMeter");

			var y:Number = 0;
			for (var i:int = 0; i < _actors.length; i++) {
				var actor:Object = _actors[i];
				var name:TextField = _theme.makeText(16, _theme.color("text"));
				name.width = BAR_W;
				name.height = 22;
				name.y = y - 3;
				name.text = String(actor.name).toUpperCase();
				var tf:TextFormat = name.getTextFormat();
				tf.letterSpacing = 1;
				tf.align = _mirror ? TextFormatAlign.RIGHT : TextFormatAlign.LEFT;
				name.setTextFormat(tf);
				_names.addChild(name);

				var bar:Shape = new Shape();
				bar.y = y + NAME_H;
				Theme.fill(bar.graphics, _theme.colors("meterTrack"), 0, 0, BAR_W, BAR_H, 0, 1, BAR_H / 2);
				var value:Number = Number(actor.meter);
				if (value > 0) {
					var key:String = actor.sex == "male" ? "meterMale" : actor.sex == "female" ? "meterFemale" : "meterOther";
					var w:Number = Math.max(BAR_H, BAR_W * Math.min(1, value));
					Theme.fill(bar.graphics, _theme.colors(key), _mirror ? BAR_W - w : 0, 0, w, BAR_H, _mirror ? Math.PI : 0, 1, BAR_H / 2);
				}
				_names.addChild(bar);
				y += NAME_H + BAR_H + ROW_GAP;
			}

			drawSpeed();

			if (_mirror) {
				_speedMeter.x = 0;
				_names.x = SPEED_W + SPEED_GAP;
			} else {
				_names.x = 0;
				_speedMeter.x = BAR_W + SPEED_GAP;
			}
		}

		private function drawSpeed():void
		{
			while (_speedMeter.numChildren > 0) {
				_speedMeter.removeChildAt(0);
			}
			var total:Number = Math.max(90, rowsHeight());
			var label:TextField = _theme.makeText(12, _theme.color("textMuted"));
			label.width = 40;
			label.height = 16;
			label.x = (SPEED_W - 40) / 2;
			label.y = -3;
			label.text = "SPD";
			var tf:TextFormat = label.getTextFormat();
			tf.align = TextFormatAlign.CENTER;
			label.setTextFormat(tf);
			_speedMeter.addChild(label);

			var value:TextField = _theme.makeText(13, _theme.color("accent"));
			value.width = 40;
			value.height = 18;
			value.x = (SPEED_W - 40) / 2;
			value.y = total - 15;
			value.text = _speedCount > 0 ? (_speed + 1) + "/" + _speedCount : "-";
			tf = value.getTextFormat();
			tf.align = TextFormatAlign.CENTER;
			value.setTextFormat(tf);
			_speedMeter.addChild(value);

			// One segment per speed, filled from the bottom; the colors run
			// through the theme's speed gradient from slowest to fastest.
			var segs:int = Math.max(1, _speedCount);
			var top:Number = 14;
			var height:Number = total - 14 - 18;
			var gap:Number = 3;
			var segH:Number = (height - gap * (segs - 1)) / segs;
			var stops:Array = _theme.colors("speed");
			var track:Array = _theme.colors("meterTrack");
			var shape:Shape = new Shape();
			for (var level:int = 0; level < segs; level++) {
				var y:Number = top + height - (level + 1) * segH - level * gap;
				var filled:Boolean = _speedCount > 0 && level <= _speed;
				if (filled) {
					shape.graphics.beginFill(Theme.sample(stops, segs > 1 ? level / (segs - 1) : 1), stops[0].a);
				} else {
					shape.graphics.beginFill(track[0].c, track[0].a);
				}
				shape.graphics.drawRoundRect(0, y, SPEED_W, segH, 4, 4);
				shape.graphics.endFill();
			}
			_speedMeter.addChild(shape);
		}
	}
}
