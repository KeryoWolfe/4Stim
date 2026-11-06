package hud
{
	import flash.display.Shape;
	import flash.display.Sprite;
	import flash.text.TextField;
	import flash.text.TextFormat;
	import flash.text.TextFormatAlign;

	// The actor name bars and, beside them, the vertical speed meter.
	// Actors are { role, name, sex, isPlayer, meter } (HUD_API.md); meter -1
	// means nothing fills the bars yet, so only the track shows. Which actors
	// get a bar and their colors follow OStim's bar settings (setOptions):
	// by default every actor, colored by role as OStim's bars (the first
	// blue, the second pink, the rest purple), or by sex with matchGender.
	public class Meters extends Sprite
	{
		private static const BAR_W:Number = 220;
		private static const BAR_H:Number = 7;
		private static const NAME_H:Number = 19;  // name line + gap above the bar
		private static const ROW_GAP:Number = 6;
		private static const SPEED_GAP:Number = 20;
		private static const SPEED_W:Number = 9;
		// More than ROWS actors: two columns, each bar half as wide, the first
		// column nearest the HUD's corner (4Stim's layout for group scenes):
		// three rows a column up to six actors, more rows past that, drawn
		// tighter (COMPACT_*) so they still fit under the HUD's line.
		private static const ROWS:int = 3;
		private static const COLS:int = 2;
		private static const COL_GAP:Number = 14;
		private static const COMPACT_NAME_H:Number = 16;
		private static const COMPACT_BAR_H:Number = 6;
		private static const COMPACT_ROW_GAP:Number = 3;

		private var _theme:Theme;
		private var _mirror:Boolean = false;
		private var _actors:Array = [];
		private var _speed:int = 0;
		private var _speedCount:int = 0;
		private var _speedLabel:String = "";  // OStim's speed number, "" = none
		private var _showPlayer:Boolean = true;
		private var _showNpc:Boolean = true;
		private var _matchGender:Boolean = false;
		private var _shown:Array = [];  // the actors that get a bar

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

		// OStim's speed number for this speed (the scene's displaySpeed), ""
		// for none: the meter then shows speed / speeds.
		public function setSpeedLabel(a_label:String):void
		{
			_speedLabel = a_label != null ? a_label : "";
			redraw();
		}

		// OStim's bar settings: { player, npc, matchGender } (HUD_API.md SetBarOptions).
		public function setOptions(a_options:Object):void
		{
			if (a_options == null) {
				return;
			}
			_showPlayer = a_options.player !== false;
			_showNpc = a_options.npc !== false;
			_matchGender = a_options.matchGender === true;
			redraw();
		}

		// Rows in a column: all the bars up to ROWS, then half of them (two columns).
		private function rowCount():int
		{
			return _shown.length <= ROWS ? Math.max(1, _shown.length) : Math.max(ROWS, Math.ceil(_shown.length / COLS));
		}

		private function compact():Boolean { return rowCount() > ROWS; }
		private function nameH():Number { return compact() ? COMPACT_NAME_H : NAME_H; }
		private function barH():Number { return compact() ? COMPACT_BAR_H : BAR_H; }
		private function rowGap():Number { return compact() ? COMPACT_ROW_GAP : ROW_GAP; }

		private function rowsHeight():Number
		{
			var rows:int = rowCount();
			return rows * (nameH() + barH()) + (rows - 1) * rowGap();
		}

		private function columns():int
		{
			return Math.max(1, Math.ceil(_shown.length / rowCount()));
		}

		private function barWidth():Number
		{
			return _shown.length > ROWS ? BAR_W / 2 : BAR_W;
		}

		// The width of all the bar columns together.
		private function namesWidth():Number
		{
			var cols:int = columns();
			return cols * barWidth() + (cols - 1) * COL_GAP;
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

			_shown = [];
			for (var a:int = 0; a < _actors.length; a++) {
				if (_actors[a].isPlayer === true ? _showPlayer : _showNpc) {
					_shown.push(_actors[a]);
				}
			}

			var barW:Number = barWidth();
			var cols:int = columns();
			for (var i:int = 0; i < _shown.length; i++) {
				var actor:Object = _shown[i];
				var rows:int = rowCount();
				var col:int = int(i / rows);
				var x:Number = (_mirror ? cols - 1 - col : col) * (barW + COL_GAP);
				var y:Number = (i % rows) * (nameH() + barH() + rowGap());
				var name:TextField = _theme.makeText(compact() ? 12 : _shown.length > ROWS ? 14 : 16, _theme.color("text"));
				name.width = barW;
				name.height = 22;
				name.x = x;
				name.y = y - 3;
				name.text = String(actor.name).toUpperCase();
				var tf:TextFormat = name.getTextFormat();
				tf.letterSpacing = 1;
				tf.align = _mirror ? TextFormatAlign.RIGHT : TextFormatAlign.LEFT;
				name.setTextFormat(tf);
				_names.addChild(name);

				var bar:Shape = new Shape();
				bar.x = x;
				var bh:Number = barH();
				bar.y = y + nameH();
				Theme.fill(bar.graphics, _theme.colors("meterTrack"), 0, 0, barW, bh, 0, 1, bh / 2);
				var value:Number = Number(actor.meter);
				if (value > 0) {
					var key:String = _matchGender ? (actor.sex == "male" ? "meterMale" : actor.sex == "female" ? "meterFemale" : "meterOther") :
					                 (int(actor.role) == 0 ? "meterMale" : int(actor.role) == 1 ? "meterFemale" : "meterOther");
					var w:Number = Math.max(bh, barW * Math.min(1, value));
					Theme.fill(bar.graphics, _theme.colors(key), _mirror ? barW - w : 0, 0, w, bh, _mirror ? Math.PI : 0, 1, bh / 2);
				}
				_names.addChild(bar);
			}

			drawSpeed();

			if (_mirror) {
				_speedMeter.x = 0;
				_names.x = SPEED_W + SPEED_GAP;
			} else {
				_names.x = 0;
				_speedMeter.x = namesWidth() + SPEED_GAP;
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
			label.text = Strings.t("$4Stim_Speed", "SPD");
			var tf:TextFormat = label.getTextFormat();
			tf.align = TextFormatAlign.CENTER;
			label.setTextFormat(tf);
			_speedMeter.addChild(label);

			var value:TextField = _theme.makeText(13, _theme.color("accent"));
			value.width = 40;
			value.height = 18;
			value.x = (SPEED_W - 40) / 2;
			value.y = total - 15;
			value.text = _speedCount <= 0 ? "-" : _speedLabel != "" ? _speedLabel : (_speed + 1) + "/" + _speedCount;
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
