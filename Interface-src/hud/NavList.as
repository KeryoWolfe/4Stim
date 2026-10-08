package hud
{
	import flash.display.Shape;
	import flash.display.Sprite;
	import flash.text.TextField;
	import flash.text.TextFieldAutoSize;
	import flash.text.TextFormat;
	import flash.text.TextFormatAlign;

	// The list inside the L separator: a header (the tab's name), then rows
	// with an icon tile and a label, pinned to the bottom. The background
	// fades out to the right and at the top, like OStim's. Entries are
	// { id, label, icon, kind } objects (HUD_API.md); kind "disabled" rows
	// can't be picked.
	public class NavList extends Sprite
	{
		public static const ROW_H:Number = 40;
		public static const HEADER_H:Number = 31;
		private static const FADE:Number = 64;   // height of the top fade
		private static const TILE:Number = 40;
		private static const ICON:Number = 36;

		private var _w:Number;
		private var _h:Number;
		private var _visible:int;
		private var _theme:Theme;
		private var _mirror:Boolean = false;

		private var _title:TextField;
		private var _rule:Shape;
		private var _bg:Shape;
		private var _rows:Array = [];   // { sprite, bg, tile, icon, label }
		private var _empty:TextField;

		private var _entries:Array = [];
		private var _emptyText:String = "";
		private var _selected:int = 0;
		private var _top:int = 0;
		private var _focused:Boolean = false;

		public function NavList(a_width:Number, a_height:Number)
		{
			super();
			_w = a_width;
			_h = a_height;
			_visible = Math.max(1, int((_h - HEADER_H) / ROW_H));
			mouseEnabled = false;
			mouseChildren = false;
		}

		// ---- Setup ----

		public function setTheme(a_theme:Theme, a_mirror:Boolean):void
		{
			_theme = a_theme;
			_mirror = a_mirror;
			while (numChildren > 0) {
				removeChildAt(0);
			}
			_rows = [];

			_bg = new Shape();
			addChild(_bg);

			_title = _theme.makeText(15, _theme.color("textMuted"));
			_title.alpha = _theme.alpha("textMuted");
			_title.x = 4;
			_title.y = 2;
			_title.width = _w - 8;
			_title.height = 24;
			addChild(_title);

			_rule = new Shape();
			addChild(_rule);

			for (var i:int = 0; i < _visible; i++) {
				var row:Object = {};
				row.sprite = new Sprite();
				row.bg = new Shape();
				row.tile = new Shape();
				row.icon = new IconSlot(ICON);
				row.label = _theme.makeText(17, _theme.color("text"));
				row.label.height = 26;
				row.label.width = _w - TILE - 22;
				row.sprite.addChild(row.bg);
				row.sprite.addChild(row.tile);
				row.sprite.addChild(row.icon);
				row.sprite.addChild(row.label);
				addChild(row.sprite);
				_rows.push(row);
			}

			_empty = _theme.makeText(15, _theme.color("textMuted"));
			_empty.width = _w - 24;
			_empty.height = 24;
			_empty.alpha = 0.8;
			addChild(_empty);

			drawStatic();
			refresh();
		}

		public function setTitle(a_title:String):void
		{
			if (_title != null) {
				_title.text = a_title.toUpperCase();
				spaceLetters(_title, 2);
			}
		}

		public function setEntries(a_entries:Array, a_emptyText:String, a_keepSelection:Boolean = false):void
		{
			var keepID:String = a_keepSelection && selectedEntry != null ? String(selectedEntry.id) : null;
			_entries = a_entries != null ? a_entries : [];
			_emptyText = a_emptyText;
			_selected = 0;
			if (keepID != null) {
				for (var i:int = 0; i < _entries.length; i++) {
					if (String(_entries[i].id) == keepID) {
						_selected = i;
						break;
					}
				}
			}
			_top = 0;
			scrollToSelection();
			refresh();
		}

		public function set focused(a_focused:Boolean):void
		{
			_focused = a_focused;
			refresh();
		}

		public function get selectedEntry():Object
		{
			return _selected < _entries.length ? _entries[_selected] : null;
		}

		public function move(a_delta:int):void
		{
			if (_entries.length == 0) {
				return;
			}
			_selected = Math.max(0, Math.min(_entries.length - 1, _selected + a_delta));
			scrollToSelection();
			refresh();
		}

		// ---- Drawing ----

		private function scrollToSelection():void
		{
			if (_selected < _top) {
				_top = _selected;
			} else if (_selected >= _top + _visible) {
				_top = _selected - _visible + 1;
			}
			// Keep the selection out of the faded top row while there's more above.
			if (_top > 0 && _selected == _top) {
				_top = Math.max(0, _top - 1);
			}
		}

		private function drawStatic():void
		{
			var listTop:Number = HEADER_H;
			var listH:Number = _h - HEADER_H;
			var rot:Number = _mirror ? Math.PI : 0;

			// Background: fades out to the right (left when mirrored), and in
			// steps over the top FADE pixels.
			var bgStops:Array = _theme.colors("navBackground");
			var base:Object = bgStops[0];
			var stops:Array = [
				{ c: base.c, a: base.a },
				{ c: base.c, a: base.a * 0.8 },
				{ c: base.c, a: 0 }
			];
			var g:* = _bg.graphics;
			g.clear();
			var steps:int = 16;
			var stepH:Number = FADE / steps;  // whole pixels: overlapping strips would show as lines
			for (var s:int = 0; s < steps; s++) {
				Theme.fill(g, stops, 0, listTop + s * stepH, _w, stepH, rot, (s + 1) / (steps + 1));
			}
			Theme.fill(g, stops, 0, listTop + FADE, _w, listH - FADE, rot);

			// Header rule: the separator colors, fading out.
			var ruleStops:Array = _theme.colors("separator").concat();
			var last:Object = ruleStops[ruleStops.length - 1];
			ruleStops.push({ c: last.c, a: 0 });
			_rule.graphics.clear();
			Theme.fill(_rule.graphics, ruleStops, 0, HEADER_H - 5, _w, 1, rot);
		}

		private function refresh():void
		{
			if (_theme == null) {
				return;
			}
			var count:int = _entries.length;
			var shown:int = Math.min(_visible, count - _top);
			var firstSlot:int = _visible - shown;  // rows sit at the bottom
			var listTop:Number = HEADER_H;
			var listH:Number = _h - HEADER_H;
			var rot:Number = _mirror ? Math.PI : 0;

			for (var slot:int = 0; slot < _visible; slot++) {
				var row:Object = _rows[slot];
				var index:int = _top + slot - firstSlot;
				if (slot < firstSlot || index >= count) {
					row.sprite.visible = false;
					continue;
				}
				var entry:Object = _entries[index];
				var selected:Boolean = _focused && index == _selected;
				var disabled:Boolean = entry.kind == "disabled";
				var y:Number = listTop + listH - (_visible - slot) * ROW_H;
				row.sprite.visible = true;
				row.sprite.y = y;

				var bg:* = row.bg.graphics;
				bg.clear();
				if (selected) {
					var sel:Array = _theme.colors("navSelected");
					var glow:Array = [
						{ c: sel[0].c, a: 0.38 },
						{ c: sel[sel.length - 1].c, a: 0.18 },
						{ c: sel[sel.length - 1].c, a: 0 }
					];
					Theme.fill(bg, glow, 0, 0, _w, ROW_H, rot);
					bg.beginFill(_theme.color("accent"), _theme.alpha("accent"));
					bg.drawRect(_mirror ? _w - 3 : 0, 0, 3, ROW_H);
					bg.endFill();
				}

				var tileX:Number = _mirror ? _w - TILE - 3 : 3;
				var tg:* = row.tile.graphics;
				tg.clear();
				tg.beginFill(_theme.color("iconTile"), _theme.alpha("iconTile"));
				tg.drawRect(tileX, 0, TILE, ROW_H);
				tg.endFill();
				row.icon.x = tileX + (TILE - ICON) / 2;
				row.icon.y = (ROW_H - ICON) / 2;
				row.icon.load(entry.icon != null ? String(entry.icon) : "");
				row.icon.tint = null;

				var label:TextField = row.label;
				label.text = String(entry.label).toUpperCase();
				spaceLetters(label, 1);
				var tf:TextFormat = label.getTextFormat();
				tf.align = _mirror ? TextFormatAlign.RIGHT : TextFormatAlign.LEFT;
				label.setTextFormat(tf);
				label.textColor = selected ? _theme.color("navSelectedText") : _theme.color("text");
				label.x = _mirror ? 8 : tileX + TILE + 14;
				label.y = (ROW_H - 24) / 2;
				label.alpha = disabled ? 0.45 : 1;

				// Top fade: rows near the top of the list fade out.
				var mid:Number = y - listTop + ROW_H / 2;
				row.sprite.alpha = selected ? 1 : Math.min(1, mid / FADE + 0.15);
			}

			_empty.visible = count == 0 && _emptyText != "";
			_empty.text = _emptyText;
			_empty.x = _mirror ? 12 : TILE + 20;
			_empty.y = _h - ROW_H + 8;
			alpha = _focused ? 1 : 0.85;
		}

		private static function spaceLetters(a_field:TextField, a_spacing:Number):void
		{
			var tf:TextFormat = a_field.getTextFormat();
			tf.letterSpacing = a_spacing;
			a_field.setTextFormat(tf);
		}
	}
}
