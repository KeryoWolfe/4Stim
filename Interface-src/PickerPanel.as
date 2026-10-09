package
{
	import flash.display.Graphics;
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
		// A small panel at the top center (like OStim's menus), not a
		// screen-filling list.
		// It grows with the list, up to VISIBLE_ROWS rows.
		private static const PANEL_W:Number = 300;
		private static const PAD:Number = 8;
		private static const ROW_H:Number = 20;
		private static const VISIBLE_ROWS:int = 8;
		private static const TOP_Y:Number = 30;
		private static const TITLE_H:Number = 22;
		private static const LIST_Y:Number = PAD + TITLE_H + 4;
		private static const DETAIL_H:Number = 17;
		private static const FOOTER_H:Number = 15;

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
		private var _frame:Shape;

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
			x = Math.round((STAGE_W - PANEL_W) / 2);
			y = TOP_Y;

			// Background and dividers, redrawn to fit the list (layout()).
			_frame = new Shape();
			addChild(_frame);

			_title = makeText(16, GREEN);
			_title.x = PAD;
			_title.y = PAD;
			_title.width = PANEL_W - PAD * 2;
			_title.height = TITLE_H;
			_title.text = "4Stim";
			addChild(_title);

			for (var i:int = 0; i < VISIBLE_ROWS; i++) {
				var row:Sprite = new Sprite();
				row.x = PAD;
				row.y = LIST_Y + i * ROW_H;
				row.buttonMode = true;
				row.mouseChildren = false;

				var bg:Shape = new Shape();
				bg.graphics.beginFill(GREEN, 1);
				bg.graphics.drawRect(0, 0, PANEL_W - PAD * 2 - 8, ROW_H - 2);
				bg.graphics.endFill();
				row.addChild(bg);

				var label:TextField = makeText(13, GREEN);
				label.x = 4;
				label.y = 0;
				label.width = PANEL_W - PAD * 2 - 16;
				label.height = ROW_H;
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

			_empty = makeText(13, GREEN);
			_empty.x = PAD + 4;
			_empty.y = LIST_Y;
			_empty.width = PANEL_W - PAD * 2 - 8;
			_empty.height = ROW_H;
			_empty.text = "No scenes available for this pairing.";
			_empty.visible = false;
			addChild(_empty);

			_detail = makeText(11, GREEN);
			_detail.x = PAD;
			_detail.width = PANEL_W - PAD * 2;
			_detail.height = DETAIL_H;
			addChild(_detail);

			_footer = makeText(10, GREEN);
			_footer.x = PAD;
			_footer.width = PANEL_W - PAD * 2;
			_footer.height = FOOTER_H;
			_footer.alpha = 0.6;
			_footer.text = "Enter: choose   Esc: close";
			addChild(_footer);

			addEventListener(MouseEvent.MOUSE_WHEEL, onWheel);
			refresh();
		}

		// Sizes the panel to a_rows rows of list.
		private function layout(a_rows:int):void
		{
			var listH:Number = Math.max(1, a_rows) * ROW_H;
			var detailY:Number = LIST_Y + listH + 4;
			var panelH:Number = detailY + DETAIL_H + FOOTER_H + PAD;

			var g:Graphics = _frame.graphics;
			g.clear();
			g.lineStyle(2, GREEN, 1);
			g.beginFill(0x000000, 0.85);
			g.drawRect(0, 0, PANEL_W, panelH);
			g.endFill();
			g.lineStyle(1, DIM, 1);
			g.moveTo(PAD, LIST_Y - 3);
			g.lineTo(PANEL_W - PAD, LIST_Y - 3);
			g.moveTo(PAD, detailY - 2);
			g.lineTo(PANEL_W - PAD, detailY - 2);

			_detail.y = detailY;
			_footer.y = detailY + DETAIL_H;
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

			layout(Math.min(count, VISIBLE_ROWS));
			_empty.visible = count == 0;
			_detail.text = count > 0 ? String(_scenes[_selected].tags) : "";

			// Scrollbar, only when the list is longer than the view.
			_scrollbar.graphics.clear();
			if (count > VISIBLE_ROWS) {
				var trackX:Number = PANEL_W - PAD - 4;
				var trackY:Number = LIST_Y;
				var trackH:Number = VISIBLE_ROWS * ROW_H;
				var thumbH:Number = Math.max(16, trackH * VISIBLE_ROWS / count);
				var thumbY:Number = trackY + (trackH - thumbH) * _top / (count - VISIBLE_ROWS);
				_scrollbar.graphics.beginFill(DIM, 1);
				_scrollbar.graphics.drawRect(trackX, trackY, 3, trackH);
				_scrollbar.graphics.endFill();
				_scrollbar.graphics.beginFill(GREEN, 1);
				_scrollbar.graphics.drawRect(trackX, thumbY, 3, thumbH);
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
