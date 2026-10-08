package hud
{
	import flash.display.Shape;
	import flash.display.Sprite;
	import flash.text.TextField;
	import flash.text.TextFormat;
	import flash.text.TextFormatAlign;

	// The column of tab buttons left of the separator. Icons are movies in
	// Data\Interface\4Stim\Icons\Tabs\<Name>.swf, tinted with the theme.
	public class TabBar extends Sprite
	{
		public static const SIZE:Number = 52;
		public static const GAP:Number = 8;
		private static const ICON:Number = 22;
		private static const ICON_DIR:String = "4Stim/Icons/Tabs/";

		private var _tabs:Array;     // { id, label, icon }
		private var _buttons:Array = [];  // { bg, icon, label }
		private var _theme:Theme;
		private var _selected:int = 0;

		public function TabBar(a_tabs:Array)
		{
			super();
			_tabs = a_tabs;
			mouseEnabled = false;
			mouseChildren = false;
		}

		public function get count():int { return _tabs.length; }
		public function get selected():int { return _selected; }
		public function get selectedID():String { return _tabs[_selected].id; }

		public function setTheme(a_theme:Theme):void
		{
			_theme = a_theme;
			while (numChildren > 0) {
				removeChildAt(0);
			}
			_buttons = [];
			for (var i:int = 0; i < _tabs.length; i++) {
				var button:Object = {};
				var holder:Sprite = new Sprite();
				holder.y = i * (SIZE + GAP);
				button.bg = new Shape();
				holder.addChild(button.bg);

				button.icon = new IconSlot(ICON);
				button.icon.x = (SIZE - ICON) / 2;
				button.icon.y = 7;
				button.icon.load(ICON_DIR + _tabs[i].icon + ".swf");
				holder.addChild(button.icon);

				var label:TextField = _theme.makeText(8, _theme.color("text"));
				label.width = SIZE;
				label.height = 16;
				label.y = SIZE - 18;
				label.text = String(_tabs[i].label).toUpperCase();
				var tf:TextFormat = label.getTextFormat();
				tf.align = TextFormatAlign.CENTER;
				tf.letterSpacing = 0;
				label.setTextFormat(tf);
				holder.addChild(label);
				button.label = label;

				addChild(holder);
				_buttons.push(button);
			}
			refresh();
		}

		public function select(a_index:int):void
		{
			_selected = (a_index + _tabs.length) % _tabs.length;
			refresh();
		}

		public function selectID(a_id:String):void
		{
			for (var i:int = 0; i < _tabs.length; i++) {
				if (_tabs[i].id == a_id) {
					select(i);
					return;
				}
			}
		}

		private function refresh():void
		{
			if (_theme == null) {
				return;
			}
			for (var i:int = 0; i < _buttons.length; i++) {
				var button:Object = _buttons[i];
				var on:Boolean = i == _selected;
				var g:* = button.bg.graphics;
				g.clear();
				var border:Array = on ? [_theme.colors("accent")[0]] : _theme.colors("tabBorder");
				g.lineStyle(2, border[0].c, border[0].a);
				Theme.begin(g, on ? _theme.colors("tabSelected") : _theme.colors("tabBackground"), 0, 0, SIZE, SIZE, Math.PI / 4);
				g.drawRect(1, 1, SIZE - 2, SIZE - 2);
				g.endFill();

				var fg:uint = on ? _theme.color("tabSelectedText") : _theme.color("text");
				button.label.textColor = fg;
				button.icon.tint = { c: fg, a: 1 };
			}
		}
	}
}
