package hud
{
	// F4SE's Scaleform functions (the "f4se" object it gives every menu's
	// root). Fallout 4's Scaleform can't open a .dds from a path, but F4SE's
	// MountImage loads one through the game's texture system and names it,
	// after which a Loader can open it as "img://<name>". F4SE unmounts a
	// menu's images when the menu closes.
	public class F4SE
	{
		public static const MENU:String = "FourStimHUDMenu";

		private static var _obj:Object = null;
		private static var _waiting:Array = [];  // callbacks for before attach()

		public static function attach(a_f4se:Object):void
		{
			if (a_f4se == null || _obj != null) {
				return;
			}
			_obj = a_f4se;
			HUDLog.write("F4SE functions attached");
			var waiting:Array = _waiting;
			_waiting = [];
			for each (var callback:Function in waiting) {
				callback();
			}
		}

		public static function get ready():Boolean { return _obj != null; }

		// Runs a_callback once F4SE has attached (now, if it already has).
		public static function whenReady(a_callback:Function):void
		{
			if (_obj != null) {
				a_callback();
			} else {
				_waiting.push(a_callback);
			}
		}

		// Ways of writing a .dds path for MountImage. The game's texture loader
		// reads paths relative to Data\Textures\, so Data\Interface\ is
		// "..\Interface\" (the first form, which works in 1.11.240). The rest
		// are kept as fallbacks: a missing file still "mounts" (as the game's
		// shared 1x1 placeholder), so a form only counts once an image bigger
		// than 1x1 comes back, and the form that works is tried first from
		// then on. a_path is relative to Data\Interface\.
		private static const FORMS:Array = [
			"..\\Interface\\$",
			"Interface\\$",
			"Data\\Interface\\$",
			"Textures\\..\\Interface\\$",
			"Textures\\$",             // a copy under Data\Textures\
			"$"
		];
		private static var _working:int = 0;
		private static var _mountCount:int = 0;
		private static var _mounted:Object = {};  // lowercased file -> img:// URL

		// The path forms to try for a_path, best first: [{ form, file }].
		public static function candidates(a_path:String):Array
		{
			var path:String = a_path.split("/").join("\\");
			var list:Array = [];
			if (_working >= 0) {
				list.push({ form: _working, file: String(FORMS[_working]).split("$").join(path) });
			}
			for (var i:int = 0; i < FORMS.length; i++) {
				if (i != _working) {
					list.push({ form: i, file: String(FORMS[i]).split("$").join(path) });
				}
			}
			return list;
		}

		// Records that a path form showed a real image.
		public static function worked(a_form:int):void
		{
			if (_working != a_form) {
				_working = a_form;
				if (a_form != 0) {
					HUDLog.write(".dds path form " + a_form + " works: " + FORMS[a_form]);
				}
			}
		}

		// Mounts a .dds (a_file as MountImage takes it) and returns the URL to
		// load it from, or null if F4SE refused. A file is mounted once per
		// menu and its URL reused: F4SE won't mount the same texture under a
		// second name (it says yes, but the second name shows nothing), so
		// two rows with the same icon, or an icon seen in an earlier scene,
		// must share the first name.
		public static function mount(a_file:String):String
		{
			if (_obj == null || _obj.MountImage == null) {
				return null;
			}
			var key:String = a_file.toLowerCase();
			if (_mounted[key] != null) {
				return _mounted[key];
			}
			var name:String = "4Stim_" + (++_mountCount);
			var ok:Boolean = false;
			try {
				ok = Boolean(_obj.MountImage(MENU, a_file, name));
			} catch (e:Error) {
				HUDLog.write("MountImage threw for " + a_file + ": " + e.message);
			}
			if (!ok) {
				return null;
			}
			_mounted[key] = "img://" + name;
			return _mounted[key];
		}

		// Drops a file that turned out not to load (the placeholder), so a
		// later try mounts it again.
		public static function forget(a_file:String):void
		{
			if (a_file != null) {
				delete _mounted[a_file.toLowerCase()];
			}
		}
	}
}
