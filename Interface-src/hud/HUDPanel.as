package hud
{
	import flash.display.MovieClip;
	import flash.display.Sprite;
	import flash.events.Event;

	// The default 4Stim HUD (Menu_mc of FourStimHUDMenu.swf). Implements HUD
	// API version 1 -- see HUD_API.md for every function here. Layout numbers
	// are from the design mockup, on a 1280x720 reference screen, measured
	// from the anchor corner (y negative = up).
	public class HUDPanel extends MovieClip
	{
		// The engine adds the plugin's functions onto this object when the menu
		// loads, so it must exist from the start.
		public var BGSCodeObj:Object = new Object();

		public static const API_VERSION:int = 1;

		private static const V_RULE:Object = { x: 70, y: -480, w: 5, h: 365 };
		private static const H_RULE:Object = { x: 70, y: -120, w: 380, h: 5 };
		private static const TABS:Object = { x: 8, y: -394 };
		private static const NAV:Object = { x: 78, y: -478, w: 372, h: 354 };
		private static const LOGO:Object = { x: 14, y: -102, size: 88 };
		private static const METERS:Object = { x: 172, y: -104, w: 249 };

		private static const TAB_LIST:Array = [
			{ id: "search", label: "Search", icon: "Search" },
			{ id: "align", label: "Align", icon: "Align" },
			{ id: "utility", label: "Utility", icon: "Utility" },
			{ id: "navigation", label: "Navigation", icon: "Navigation" }
		];

		private var _theme:Theme = new Theme(null);
		private var _mirror:Boolean = false;
		private var _screen:Object = { x1: 0, y1: 0, x2: 1280, y2: 720 };

		private var _root:Sprite = new Sprite();  // positioned on the anchor corner
		private var _frame:Frame = new Frame();
		private var _tabs:TabBar = new TabBar(TAB_LIST);
		private var _list:NavList = new NavList(NAV.w, NAV.h);
		private var _meters:Meters = new Meters();
		private var _f4seWait:int = 0;
		private var _logo:IconSlot = new IconSlot(LOGO.size);

		private var _scene:Object = { id: "", name: "", actorCount: 0, tags: [] };
		private var _navigation:Array = [];
		private var _utility:Array = [];
		private var _speed:int = 0;
		private var _speedCount:int = 0;
		private var _focused:Boolean = false;
		private var _paused:Boolean = false;
		private var _ready:Boolean = false;

		public function HUDPanel()
		{
			super();
			mouseEnabled = false;
			mouseChildren = false;
			addChild(_root);
			// Back to front: the L, tabs and list sit under the corner disc, so
			// their corners tuck behind it.
			_root.addChild(_frame);
			_root.addChild(_tabs);
			_root.addChild(_list);
			_root.addChild(_frame.disc);
			_root.addChild(_meters);
			_root.addChild(_logo);
			_logo.addEventListener(Event.COMPLETE, onLogoLoaded);
			_tabs.selectID("navigation");
			applyTheme();
			addEventListener(Event.ENTER_FRAME, onEnterFrame);
		}

		// ---- Plugin -> movie (HUD_API.md) ----

		public function GetApiVersion():int
		{
			return API_VERSION;
		}

		public function SetScreen(a_left:Number, a_top:Number, a_right:Number, a_bottom:Number):void
		{
			_screen = { x1: a_left, y1: a_top, x2: a_right, y2: a_bottom };
			position();
		}

		public function SetTheme(a_theme:Object):void
		{
			_theme = new Theme(a_theme);
			applyTheme();
		}

		public function SetScene(a_scene:Object):void
		{
			var changed:Boolean = a_scene == null || String(a_scene.id) != String(_scene.id);
			_scene = a_scene != null ? a_scene : { id: "", name: "", actorCount: 0, tags: [] };
			if (changed) {
				callLogo("onSceneChanged", [String(_scene.id), int(_scene.actorCount)]);
			}
		}

		public function SetActors(a_actors:Array):void
		{
			_meters.setActors(a_actors);
		}

		public function SetMeters(a_values:Array):void
		{
			_meters.setMeters(a_values);
		}

		public function SetSpeed(a_level:int, a_count:int):void
		{
			var changed:Boolean = a_level != _speed || a_count != _speedCount;
			_speed = a_level;
			_speedCount = a_count;
			_meters.setSpeed(a_level, a_count);
			if (changed) {
				callLogo("onSpeedChanged", [a_level, a_count]);
			}
		}

		public function SetNavigation(a_entries:Array):void
		{
			_navigation = a_entries != null ? a_entries : [];
			if (_tabs.selectedID == "navigation") {
				showTab(true);  // same list resent (e.g. after a speed change): keep the selection
			}
		}

		public function SetUtility(a_entries:Array):void
		{
			_utility = a_entries != null ? a_entries : [];
			if (_tabs.selectedID == "utility") {
				showTab(true);
			}
		}

		public function SetFocus(a_focused:Boolean):void
		{
			_focused = a_focused;
			_list.focused = a_focused;
		}

		public function SetPaused(a_paused:Boolean):void
		{
			if (a_paused == _paused) {
				return;
			}
			_paused = a_paused;
			var logo:Object = _logo.content;
			if (logo == null) {
				return;
			}
			if (hasFunction(logo, "setPaused")) {
				logo.setPaused(a_paused);
			} else if (logo is MovieClip) {
				if (a_paused) {
					MovieClip(logo).stop();
				} else {
					MovieClip(logo).play();
				}
			}
		}

		public function ProcessUserEvent(a_name:String, a_down:Boolean):Boolean
		{
			if (!_focused) {
				return false;
			}
			if (!a_down) {
				return a_name == "Up" || a_name == "Down" || a_name == "Left" || a_name == "Right" || a_name == "Accept" || a_name == "Cancel";
			}
			switch (a_name) {
				case "Up":
					_list.move(-1);
					return true;
				case "Down":
					_list.move(1);
					return true;
				case "Left":
				case "PrevTab":
					_tabs.select(_tabs.selected - 1);
					showTab(false);
					return true;
				case "Right":
				case "NextTab":
					_tabs.select(_tabs.selected + 1);
					showTab(false);
					return true;
				case "Accept":
					activate(_list.selectedEntry);
					return true;
				case "Cancel":
					code("ReleaseFocus");
					return true;
				case "SpeedUp":
					code("ChangeSpeed", 1);
					return true;
				case "SpeedDown":
					code("ChangeSpeed", -1);
					return true;
			}
			return false;
		}

		// ---- Internals ----

		private function onEnterFrame(a_event:Event):void
		{
			// F4SE normally calls root.onF4SEObjCreated; this catches a root
			// it gave the object to without calling (a replaced document class).
			if (!F4SE.ready && root != null) {
				try {
					F4SE.attach(Object(root).f4se);
				} catch (e:Error) {
				}
				if (!F4SE.ready && ++_f4seWait == 60) {
					HUDLog.write("F4SE functions not found; .dds icons can't be shown");
				}
			}
			// Tell the plugin once its functions are attached to BGSCodeObj.
			if (!_ready && hasFunction(BGSCodeObj, "Ready")) {
				_ready = true;
				HUDLog.attach(BGSCodeObj);
				BGSCodeObj.Ready();
			}
		}

		private function code(a_name:String, ...a_args):void
		{
			if (hasFunction(BGSCodeObj, a_name)) {
				BGSCodeObj[a_name].apply(BGSCodeObj, a_args);
			}
		}

		private static function hasFunction(a_object:Object, a_name:String):Boolean
		{
			try {
				return a_object != null && a_object[a_name] != null;  // null also matches undefined
			} catch (e:Error) {
			}
			return false;
		}

		private function activate(a_entry:Object):void
		{
			if (a_entry == null) {
				return;
			}
			switch (String(a_entry.kind)) {
				case "scene":
					code("Navigate", String(a_entry.id));
					break;
				case "end":
					code("EndScene");
					break;
				case "search":
					code("OpenSearch");
					break;
				case "disabled":
					break;
				default:  // Utility entries have no kind
					code("RunUtility", String(a_entry.id));
					break;
			}
		}

		// Fills the list for the selected tab.
		private function showTab(a_keepSelection:Boolean):void
		{
			switch (_tabs.selectedID) {
				case "navigation":
					_list.setTitle("Navigation");
					_list.setEntries(_navigation, "", a_keepSelection);
					break;
				case "utility":
					_list.setTitle("Utility");
					_list.setEntries(_utility, "No Utility options installed", a_keepSelection);
					break;
				case "search":
					_list.setTitle("Search");
					_list.setEntries([{ id: "search", label: "Browse all scenes", icon: "", kind: "search" }], "", false);
					break;
				case "align":
					_list.setTitle("Align");
					_list.setEntries([{ id: "align", label: "Alignment: coming soon", icon: "", kind: "disabled" }], "", false);
					break;
			}
		}

		private function px(a_x:Number, a_w:Number):Number
		{
			return _mirror ? -(a_x + a_w) : a_x;
		}

		private function applyTheme():void
		{
			_mirror = String(_theme.layout("anchor", "bottomLeft")) == "bottomRight";

			_frame.draw(_theme, _mirror,
				{ x: px(V_RULE.x, V_RULE.w), y: V_RULE.y, w: V_RULE.w, h: V_RULE.h },
				{ x: px(H_RULE.x, H_RULE.w), y: H_RULE.y, w: H_RULE.w, h: H_RULE.h });

			_tabs.setTheme(_theme);
			_tabs.x = px(TABS.x, TabBar.SIZE);
			_tabs.y = TABS.y;
			_tabs.visible = _theme.show("tabs");

			_list.setTheme(_theme, _mirror);
			_list.x = px(NAV.x, NAV.w);
			_list.y = NAV.y;
			_list.visible = _theme.show("navigation");
			_list.focused = _focused;
			showTab(true);

			_meters.setTheme(_theme, _mirror);
			_meters.x = px(METERS.x, METERS.w);
			_meters.y = METERS.y;

			_logo.x = px(LOGO.x, LOGO.size);
			_logo.y = LOGO.y;
			_logo.visible = _theme.show("logo");
			var logoPath:String = _theme.data.logoMovie != null ? String(_theme.data.logoMovie) : "";
			if (logoPath == _logo.path) {
				themeLogo();  // already loaded: just pass on the new theme
			}
			_logo.load(logoPath);

			position();
		}

		private function position():void
		{
			var scale:Number = Number(_theme.layout("scale", 1));
			var offsetX:Number = Number(_theme.layout("offsetX", 0));
			var offsetY:Number = Number(_theme.layout("offsetY", 0));
			_root.scaleX = _root.scaleY = scale > 0 ? scale : 1;
			_root.alpha = Number(_theme.layout("opacity", 1));
			_root.x = _mirror ? _screen.x2 - offsetX : _screen.x1 + offsetX;
			_root.y = _screen.y2 - offsetY;
		}

		private function onLogoLoaded(a_event:Event):void
		{
			themeLogo();
			callLogo("onSceneChanged", [String(_scene.id), int(_scene.actorCount)]);
			callLogo("onSpeedChanged", [_speed, _speedCount]);
			if (_paused) {
				_paused = false;
				SetPaused(true);
			}
		}

		// Lets the logo movie color itself (setTheme), or tints it with the
		// accent color if it can't and the theme says to.
		private function themeLogo():void
		{
			var logo:Object = _logo.content;
			if (logo == null) {
				return;
			}
			if (hasFunction(logo, "setTheme")) {
				_logo.tint = null;
				logo.setTheme(_theme.data);
			} else if (_theme.data.tintLogo === true) {
				_logo.tint = { c: _theme.color("accent"), a: 1 };
			} else {
				_logo.tint = null;
			}
		}

		private function callLogo(a_name:String, a_args:Array):void
		{
			var logo:Object = _logo.content;
			if (hasFunction(logo, a_name)) {
				try {
					logo[a_name].apply(logo, a_args);
				} catch (e:Error) {
					HUDLog.write("logo movie " + a_name + " failed: " + e.message);
				}
			}
		}
	}
}
