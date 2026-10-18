package hud
{
	import flash.display.MovieClip;
	import flash.display.Sprite;
	import flash.events.Event;
	import flash.utils.getTimer;

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
			{ id: "search", label: "Search", key: "$4Stim_TabSearch", icon: "Search" },
			{ id: "align", label: "Align", key: "$4Stim_TabAlign", icon: "Align" },
			{ id: "utility", label: "Utility", key: "$4Stim_TabUtility", icon: "Utility" },
			{ id: "navigation", label: "Navigation", key: "$4Stim_TabNavigation", icon: "Navigation" }
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
		private var _glow:ClimaxGlow = new ClimaxGlow();  // full screen, behind the panels

		private var _scene:Object = { id: "", name: "", actorCount: 0, tags: [] };
		private var _navigation:Array = [];
		private var _utility:Array = [];
		private var _align:Object = null;  // SetAlign's data
		private var _speed:int = 0;
		private var _speedCount:int = 0;
		private var _focused:Boolean = false;
		// OStim's control scheme ("columns"): Up / Down move in the list,
		// Left / Right between the list and the tab buttons. _tabFocus: the
		// tab buttons have the keys (a ring on one; it opens on Accept).
		private var _columns:Boolean = false;
		private var _tabFocus:Boolean = false;
		private var _paused:Boolean = false;
		private var _ready:Boolean = false;
		// The controller's scheme: "modern" (4Stim's: B to the tab buttons,
		// then hide; d-pad Left / Right only change an Align value) or
		// "classic" (OStim's navigation: d-pad Left / Right between the list
		// and the tab buttons, as its columns).
		private var _padClassic:Boolean = false;
		// OStim's fade: after _fadeTime seconds without input the HUD fades
		// out and hands the keys back; any of its keys brings it back.
		private var _fadeTime:Number = 10;
		private var _lastInput:int = 0;
		private var _fade:Number = 1;     // 0 faded out .. 1 shown
		private var _lastFrame:int = 0;
		private var _opacity:Number = 1;  // the theme's

		public function HUDPanel()
		{
			super();
			mouseEnabled = false;
			mouseChildren = false;
			addChild(_glow);
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
			wake();
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

		// Optional (HUD_API.md): a climax in the focused scene, a_strength 0-1;
		// a_size, how far in the glow reaches (a share of the screen's
		// smaller side, fClimaxGlowSize).
		public function PlayClimax(a_strength:Number, a_size:Number = 0.07):void
		{
			_glow.play(a_strength, _screen, a_size);
		}

		// Optional (HUD_API.md): the plugin's translations, { "$4Stim_...": text }.
		public function SetStrings(a_table:Object):void
		{
			Strings.set(a_table);
			applyTheme();  // the tab labels, titles and meters again
		}

		public function SetTheme(a_theme:Object):void
		{
			_theme = new Theme(a_theme);
			applyTheme();
		}

		public function SetScene(a_scene:Object):void
		{
			var changed:Boolean = a_scene == null || String(a_scene.id) != String(_scene.id);
			if (String(_scene.id) == "" && a_scene != null) {
				wake();  // a new scene: shown, as OStim's menu opens with it
			}
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

		// Optional (HUD_API.md): the Align tab's data. { available, role,
		// roleCount, name, x, y, z, rot, scale, bend, step }.
		public function SetAlign(a_data:Object):void
		{
			_align = a_data;
			if (_tabs.selectedID == "align") {
				showTab(true);
			}
		}

		// Optional (HUD_API.md): show tab a_id ("navigation", "utility",
		// "align", "search"), for OStim's alignment and search keys.
		public function ShowTab(a_id:String):void
		{
			wake();
			if (_tabs.selectedID == a_id) {
				return;
			}
			_tabs.selectID(a_id);
			showTab(false);
		}

		// Optional (HUD_API.md): hide the HUD, or show it again (OStim's hide
		// UI key). The climax glow still plays.
		public function SetHidden(a_hidden:Boolean):void
		{
			_root.visible = !a_hidden;
			if (!a_hidden) {
				wake();
			}
		}

		public function SetFocus(a_focused:Boolean):void
		{
			_focused = a_focused;
			if (!a_focused) {
				_tabFocus = false;
			} else {
				wake();
			}
			updateFocus();
		}

		// Optional (HUD_API.md): the controller's scheme, "modern" (4Stim's)
		// or "classic" (OStim's navigation).
		public function SetPadScheme(a_scheme:String):void
		{
			_padClassic = String(a_scheme).toLowerCase() == "classic";
		}

		// Optional (HUD_API.md): OStim's fadeTime, seconds without input
		// before the HUD fades out (and hands the keys back); 0 = never.
		public function SetFadeTime(a_seconds:Number):void
		{
			_fadeTime = a_seconds > 0 ? a_seconds : 0;
			wake();
		}

		private function wake():void
		{
			_lastInput = getTimer();
		}

		// Optional (HUD_API.md): "tabs" (Left / Right change tabs, the
		// default) or "columns" (OStim's: Left / Right move between the list
		// and the tab buttons, Up / Down move within the one that has the keys).
		public function SetControlScheme(a_scheme:String):void
		{
			_columns = String(a_scheme).toLowerCase() == "columns";
			_tabFocus = false;
			updateFocus();
		}

		private function updateFocus():void
		{
			_list.focused = _focused && !_tabFocus;
			_tabs.focused = _focused && _tabFocus;
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
				return a_name == "Up" || a_name == "Down" || a_name == "Left" || a_name == "Right" || a_name == "Accept" || a_name == "Cancel" ||
				       a_name == "Back" || a_name == "Toggle" || a_name == "PadLeft" || a_name == "PadRight";
			}
			wake();
			// The controller's d-pad Left / Right. Classic (OStim's): between
			// the list and the tab buttons, as its columns. Modern: only an
			// Align row's value (B reaches the tab buttons).
			if (a_name == "PadLeft" || a_name == "PadRight") {
				var side:String = a_name == "PadLeft" ? "Left" : "Right";
				if (_padClassic) {
					if (_tabFocus) {
						return handleTabFocus(side, true);
					}
					if (!handleColumns(side)) {
						alignChange(side);
					}
					return true;
				}
				if (!_tabFocus) {
					alignChange(side);
				}
				return true;
			}
			// The controller's B. Classic: OStim has no back key, so it hands
			// the keys back to the game (as Esc). Modern: below.
			if (a_name == "Back" && _padClassic) {
				code("ReleaseFocus");
				return true;
			}
			// OStim's Toggle key (numpad 9): between the list and the tab buttons.
			if (a_name == "Toggle") {
				if (_tabFocus) {
					leaveTabs();
				} else {
					enterTabs();
				}
				return true;
			}
			if (_tabFocus && handleTabFocus(a_name, _columns)) {
				return true;
			}
			// The controller's B (Modern): from the list to the tab buttons (Up /
			// Down pick a tab, A opens it); from the tab buttons it hides the HUD,
			// the scene going on (A shows it again).
			if (a_name == "Back") {
				enterTabs();
				return true;
			}
			if (_columns && handleColumns(a_name)) {
				return true;
			}
			// Align tab: Left / Right change the selected row's value.
			if ((a_name == "Left" || a_name == "Right") && alignChange(a_name)) {
				return true;
			}
			switch (a_name) {
				case "Up":
					_list.move(-1);
					updateRepeat();
					return true;
				case "Down":
					_list.move(1);
					updateRepeat();
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

		// Left / Right on an Align row that takes them: its value. Returns
		// whether there was one.
		private function alignChange(a_side:String):Boolean
		{
			var row:Object = _list.selectedEntry;
			if (_tabs.selectedID != "align" || row == null) {
				return false;
			}
			var dir:int = a_side == "Left" ? -1 : 1;
			switch (String(row.kind)) {
				case "alignValue":
					code("AlignAdjust", String(row.field), dir);
					return true;
				case "alignActor":
					code("AlignActor", dir);
					return true;
				case "alignStep":
					code("AlignStep", dir);
					return true;
			}
			return false;
		}

		// The keys to the tab buttons (OStim's menu selector): the ring starts
		// on the tab shown.
		private function enterTabs():void
		{
			_tabFocus = true;
			_tabs.highlight(_tabs.selected);
			updateFocus();
			code("SetHorizontalRepeat", false);
		}

		private function leaveTabs():void
		{
			_tabFocus = false;
			updateFocus();
			updateRepeat();
		}

		// The tab buttons have the keys. As OStim's menu selector, Up / Down
		// move the ring and Yes (Accept) opens that tab and goes back to the
		// list. a_columns: Left / Right as OStim's (towards the list: back to
		// it, without opening anything), else they move the ring too. B
		// (Modern) hides the HUD. Returns true if it handled a_name.
		private function handleTabFocus(a_name:String, a_columns:Boolean):Boolean
		{
			var toList:String = _mirror ? "Left" : "Right";
			switch (a_name) {
				case "Up":
				case "PrevTab":
					_tabs.highlight(_tabs.highlighted - 1);
					return true;
				case "Down":
				case "NextTab":
					_tabs.highlight(_tabs.highlighted + 1);
					return true;
				case "Left":
				case "Right":
					if (a_columns) {
						if (a_name == toList) {
							leaveTabs();
						}
						return true;
					}
					_tabs.highlight(_tabs.highlighted + (a_name == "Left" ? -1 : 1));
					return true;
				case "Accept":
					if (_tabs.highlighted != _tabs.selected) {
						_tabs.select(_tabs.highlighted);
						showTab(false);
					}
					leaveTabs();
					return true;
				case "Back":
					code("Hide");
					return true;
			}
			return false;  // Cancel, speed keys: as usual
		}

		// OStim's columns, in the list. Returns true if it handled a_name.
		// Left / Right still change a value on the Align tab's value, actor and
		// step rows (the safeguard): the tab buttons are reached from any other
		// row.
		private function handleColumns(a_name:String):Boolean
		{
			var toTabs:String = _mirror ? "Right" : "Left";
			var toList:String = _mirror ? "Left" : "Right";
			if (a_name != toTabs && a_name != toList) {
				return false;
			}
			var row:Object = _list.selectedEntry;
			var kind:String = row != null ? String(row.kind) : "";
			if (_tabs.selectedID == "align" && (kind == "alignValue" || kind == "alignActor" || kind == "alignStep")) {
				return false;  // changes the value
			}
			if (a_name == toTabs) {
				enterTabs();
			}
			return true;  // toList: already in the list
		}

		// OStim's fade: no input for _fadeTime seconds fades the HUD out (and
		// hands the keys back); wake() brings it back. Not while paused.
		private function updateFade():void
		{
			var now:int = getTimer();
			var dt:Number = _lastFrame > 0 ? Math.min((now - _lastFrame) / 1000, 0.1) : 0;
			_lastFrame = now;
			if (_paused) {
				_lastInput += int(dt * 1000);  // the clock stops with the game
			}
			var idle:Number = (now - _lastInput) / 1000;
			var target:Number = _fadeTime > 0 && idle >= _fadeTime ? 0 : 1;
			if (_fade == target) {
				return;
			}
			_fade = target > _fade ? Math.min(1, _fade + dt / 0.2) : Math.max(0, _fade - dt / 0.6);
			_root.alpha = _opacity * _fade;
			if (_fade == 0 && _focused) {
				code("ReleaseFocus");
			}
		}

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
			updateFade();
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
				case "alignValue":
					break;
				case "alignActor":
					code("AlignActor", 1);
					break;
				case "alignStep":
					code("AlignStep", 1);
					break;
				case "alignReset":
					code("AlignReset");
					break;
				case "speedUp":  // OStim's speed buttons, as rows (the plugin sends them)
					code("ChangeSpeed", 1);
					break;
				case "speedDown":
					code("ChangeSpeed", -1);
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
					_list.setTitle(Strings.t("$4Stim_TabNavigation", "Navigation"));
					_list.setEntries(_navigation, "", a_keepSelection);
					break;
				case "utility":
					_list.setTitle(Strings.t("$4Stim_TabUtility", "Utility"));
					_list.setEntries(_utility, Strings.t("$4Stim_NoUtility", "No Utility options installed"), a_keepSelection);
					break;
				case "search":
					_list.setTitle(Strings.t("$4Stim_TabSearch", "Search"));
					_list.setEntries([{ id: "search", label: Strings.t("$4Stim_BrowseAllScenes", "Browse all scenes"), icon: "", kind: "search" }], "", false);
					break;
				case "align":
					_list.setTitle(Strings.t("$4Stim_TabAlign", "Align"));
					_list.setEntries(alignEntries(), "", a_keepSelection);
					break;
			}
			updateRepeat();
		}

		// The Align tab's rows, from SetAlign's data. Left / Right change the
		// value of the row selected (ProcessUserEvent).
		private function alignEntries():Array
		{
			if (_align == null || _align.available !== true) {
				return [{ id: "align_none", label: Strings.t("$4Stim_NothingToAlign", "Nothing to align"), icon: "", kind: "disabled" }];
			}
			var name:String = String(_align.name);
			var count:int = int(_align.roleCount);
			return [
				{ id: "align_actor", label: "< " + name + (count > 1 ? " (" + (int(_align.role) + 1) + "/" + count + ")" : "") + " >", icon: "", kind: "alignActor" },
				{ id: "align_x", label: Strings.t("$4Stim_AlignX", "Left / right") + "   " + signed(Number(_align.x), 1), icon: "", kind: "alignValue", field: "x" },
				{ id: "align_y", label: Strings.t("$4Stim_AlignY", "Back / forward") + "   " + signed(Number(_align.y), 1), icon: "", kind: "alignValue", field: "y" },
				{ id: "align_z", label: Strings.t("$4Stim_AlignZ", "Down / up") + "   " + signed(Number(_align.z), 1), icon: "", kind: "alignValue", field: "z" },
				{ id: "align_rot", label: Strings.t("$4Stim_AlignTurn", "Turn") + "   " + signed(Number(_align.rot), 1) + " " + Strings.t("$4Stim_Degrees", "deg"), icon: "", kind: "alignValue", field: "rot" },
				{ id: "align_scale", label: Strings.t("$4Stim_AlignSize", "Size") + "   " + Number(_align.scale).toFixed(3), icon: "", kind: "alignValue", field: "scale" },
				{ id: "align_bend", label: Strings.t("$4Stim_AlignBend", "Penis bend") + "   " + signed(Number(_align.bend), 0), icon: "", kind: "alignValue", field: "bend" },
				{ id: "align_step", label: Strings.t("$4Stim_AlignStep", "Step") + "   < " + Number(_align.step) + " >", icon: "", kind: "alignStep" },
				{ id: "align_reset", label: Strings.t("$4Stim_AlignReset", "Reset") + " " + name, icon: "", kind: "alignReset" }
			];
		}

		private static function signed(a_value:Number, a_digits:int):String
		{
			var text:String = a_value.toFixed(a_digits);
			return a_value > 0 ? "+" + text : text;
		}

		// Hold Left / Right to keep changing a value: asks the plugin to repeat
		// them while an Align row that takes them is selected.
		private function updateRepeat():void
		{
			var row:Object = _list.selectedEntry;
			var kind:String = row != null ? String(row.kind) : "";
			code("SetHorizontalRepeat", !_tabFocus && _tabs.selectedID == "align" && kind == "alignValue");
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
			_opacity = Number(_theme.layout("opacity", 1));
			_root.alpha = _opacity * _fade;
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
