package
{
	import flash.display.MovieClip;
	import flash.display.Shape;
	import flash.display.Sprite;
	import flash.events.Event;
	import flash.events.MouseEvent;
	import flash.text.TextField;
	import flash.text.TextFormat;

	// The scene picker. Talks to the 4Stim plugin through BGSCodeObj, which
	// the engine attaches when the menu loads:
	//   BGSCodeObj.RequestScenes()   ask for the list (answered via SetScenes)
	//   BGSCodeObj.PickScene(id)     start the chosen scene
	//   BGSCodeObj.CloseMenu()       close without picking
	// Keyboard/controller navigation arrives through ProcessUserEvent.
	public class PickerPanel extends MovieClip
	{
		// The engine adds the plugin's functions onto this object when the menu
		// loads, so it must exist first -- as in the game's own menus.
		public var BGSCodeObj:Object = new Object();

		private static const FONT:String = "$MAIN_Font";
		private static const GREEN:uint = 0x1AFF80;
		private static const DIM:uint = 0x0D5C33;
		private static const STAGE_W:Number = 1280;
		private static const STAGE_H:Number = 720;
		private static const PANEL_W:Number = 560;
		private static const PAD:Number = 16;
		private static const ROW_H:Number = 30;
		private static const VISIBLE_ROWS:int = 14;

		private var _scenes:Array = [];
		private var _selected:int = 0;
		private var _top:int = 0;
		private var _requested:Boolean = false;

		private var _title:TextField;
		private var _detail:TextField;
		private var _footer:TextField;
		private var _empty:TextField;
		private var _rows:Array = [];
		private var _rowBgs:Array = [];
		private var _rowLabels:Array = [];
		private var _scrollbar:Shape;

		public function PickerPanel()
		{
			super();
			build();
			addEventListener(Event.ENTER_FRAME, onEnterFrame);
		}

		// ---- Called by the plugin ----

		public function SetScenes(a_list:Array, a_title:String):void
		{
			_scenes = a_list != null ? a_list : [];
			_selected = 0;
			_top = 0;
			_title.text = a_title;
			refresh();
		}

		public function ProcessUserEvent(a_name:String, a_down:Boolean):Boolean
		{
			if (!a_down) {
				return false;
			}
			switch (a_name) {
				case "Up":     move(-1); return true;
				case "Down":   move(1); return true;
				case "Left":   move(-VISIBLE_ROWS); return true;
				case "Right":  move(VISIBLE_ROWS); return true;
				case "Accept": pick(); return true;
				case "Cancel": close(); return true;
			}
			return false;
		}

		// ---- Internals ----

		private function onEnterFrame(a_event:Event):void
		{
			// Backup only: the plugin sends the list itself when the menu
			// loads. Ask once the engine has attached the functions.
			if (!_requested && hasCodeFunction("RequestScenes")) {
				_requested = true;
				removeEventListener(Event.ENTER_FRAME, onEnterFrame);
				BGSCodeObj.RequestScenes();
			}
		}

		private function hasCodeFunction(a_name:String):Boolean
		{
			return BGSCodeObj != null && BGSCodeObj[a_name] != null;  // null also matches undefined
		}

		private function build():void
		{
			var listH:Number = VISIBLE_ROWS * ROW_H;
			var panelH:Number = PAD + 36 + 8 + listH + 8 + 26 + 8 + 24 + PAD;
			x = Math.round((STAGE_W - PANEL_W) / 2);
			y = Math.round((STAGE_H - panelH) / 2);

			graphics.lineStyle(2, GREEN, 1);
			graphics.beginFill(0x000000, 0.85);
			graphics.drawRect(0, 0, PANEL_W, panelH);
			graphics.endFill();

			_title = makeText(24, GREEN);
			_title.x = PAD;
			_title.y = PAD;
			_title.width = PANEL_W - PAD * 2;
			_title.height = 36;
			_title.text = "4Stim";
			addChild(_title);

			var listY:Number = PAD + 36 + 8;
			graphics.lineStyle(1, DIM, 1);
			graphics.moveTo(PAD, listY - 4);
			graphics.lineTo(PANEL_W - PAD, listY - 4);

			for (var i:int = 0; i < VISIBLE_ROWS; i++) {
				var row:Sprite = new Sprite();
				row.x = PAD;
				row.y = listY + i * ROW_H;
				row.buttonMode = true;
				row.mouseChildren = false;

				var bg:Shape = new Shape();
				bg.graphics.beginFill(GREEN, 1);
				bg.graphics.drawRect(0, 0, PANEL_W - PAD * 2 - 10, ROW_H - 2);
				bg.graphics.endFill();
				row.addChild(bg);

				var label:TextField = makeText(18, GREEN);
				label.x = 8;
				label.y = 2;
				label.width = PANEL_W - PAD * 2 - 26;
				label.height = ROW_H - 2;
				row.addChild(label);

				row.addEventListener(MouseEvent.MOUSE_OVER, onRowOver);
				row.addEventListener(MouseEvent.CLICK, onRowClick);
				addChild(row);
				_rows.push(row);
				_rowBgs.push(bg);
				_rowLabels.push(label);
			}

			_scrollbar = new Shape();
			addChild(_scrollbar);

			_empty = makeText(18, GREEN);
			_empty.x = PAD + 8;
			_empty.y = listY + 8;
			_empty.width = PANEL_W - PAD * 2 - 16;
			_empty.height = 28;
			_empty.text = "No scenes available for this pairing.";
			_empty.visible = false;
			addChild(_empty);

			var detailY:Number = listY + listH + 8;
			graphics.moveTo(PAD, detailY - 4);
			graphics.lineTo(PANEL_W - PAD, detailY - 4);

			_detail = makeText(15, GREEN);
			_detail.x = PAD;
			_detail.y = detailY;
			_detail.width = PANEL_W - PAD * 2;
			_detail.height = 26;
			addChild(_detail);

			_footer = makeText(14, GREEN);
			_footer.x = PAD;
			_footer.y = detailY + 26 + 8;
			_footer.width = PANEL_W - PAD * 2;
			_footer.height = 24;
			_footer.alpha = 0.7;
			_footer.text = "Enter: start    Esc: close    Arrows / mouse wheel: browse";
			addChild(_footer);

			addEventListener(MouseEvent.MOUSE_WHEEL, onWheel);
			refresh();
		}

		// Text in the game's UI font. The font name maps through the game's
		// font library; if it can't be found, fall back to a device font so
		// the text stays visible rather than blank.
		private function makeText(a_size:Number, a_color:uint):TextField
		{
			var field:TextField = new TextField();
			field.defaultTextFormat = new TextFormat(FONT, a_size, a_color);
			field.embedFonts = true;
			field.selectable = false;
			field.mouseEnabled = false;
			field.text = "Ag";
			if (field.textWidth == 0) {
				field.embedFonts = false;
				field.defaultTextFormat = new TextFormat("_sans", a_size, a_color);
			}
			field.text = "";
			return field;
		}

		private function refresh():void
		{
			var count:int = _scenes.length;
			for (var i:int = 0; i < VISIBLE_ROWS; i++) {
				var index:int = _top + i;
				var row:Sprite = _rows[i];
				var label:TextField = _rowLabels[i];
				var bg:Shape = _rowBgs[i];
				if (index < count) {
					row.visible = true;
					label.text = String(_scenes[index].name);
					var selected:Boolean = index == _selected;
					bg.visible = selected;
					label.textColor = selected ? 0x000000 : GREEN;
				} else {
					row.visible = false;
				}
			}

			_empty.visible = count == 0;
			_detail.text = count > 0 ? String(_scenes[_selected].tags) : "";

			// Scrollbar, only when the list is longer than the view.
			_scrollbar.graphics.clear();
			if (count > VISIBLE_ROWS) {
				var trackX:Number = PANEL_W - PAD - 6;
				var trackY:Number = PAD + 36 + 8;
				var trackH:Number = VISIBLE_ROWS * ROW_H;
				var thumbH:Number = Math.max(20, trackH * VISIBLE_ROWS / count);
				var thumbY:Number = trackY + (trackH - thumbH) * _top / (count - VISIBLE_ROWS);
				_scrollbar.graphics.beginFill(DIM, 1);
				_scrollbar.graphics.drawRect(trackX, trackY, 4, trackH);
				_scrollbar.graphics.endFill();
				_scrollbar.graphics.beginFill(GREEN, 1);
				_scrollbar.graphics.drawRect(trackX, thumbY, 4, thumbH);
				_scrollbar.graphics.endFill();
			}
		}

		private function move(a_delta:int):void
		{
			var count:int = _scenes.length;
			if (count == 0) {
				return;
			}
			_selected = Math.max(0, Math.min(count - 1, _selected + a_delta));
			if (_selected < _top) {
				_top = _selected;
			} else if (_selected >= _top + VISIBLE_ROWS) {
				_top = _selected - VISIBLE_ROWS + 1;
			}
			refresh();
		}

		private function onWheel(a_event:MouseEvent):void
		{
			var count:int = _scenes.length;
			if (count <= VISIBLE_ROWS) {
				return;
			}
			_top = Math.max(0, Math.min(count - VISIBLE_ROWS, _top - (a_event.delta > 0 ? 3 : -3)));
			refresh();
		}

		private function onRowOver(a_event:MouseEvent):void
		{
			var index:int = _top + _rows.indexOf(a_event.currentTarget);
			if (index < _scenes.length && index != _selected) {
				_selected = index;
				refresh();
			}
		}

		private function onRowClick(a_event:MouseEvent):void
		{
			var index:int = _top + _rows.indexOf(a_event.currentTarget);
			if (index < _scenes.length) {
				_selected = index;
				pick();
			}
		}

		private function pick():void
		{
			if (_scenes.length > 0 && hasCodeFunction("PickScene")) {
				BGSCodeObj.PickScene(String(_scenes[_selected].id));
			}
		}

		private function close():void
		{
			if (hasCodeFunction("CloseMenu")) {
				BGSCodeObj.CloseMenu();
			}
		}
	}
}
