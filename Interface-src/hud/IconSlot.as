package hud
{
	import flash.display.Bitmap;
	import flash.display.DisplayObject;
	import flash.display.Loader;
	import flash.display.Sprite;
	import flash.events.Event;
	import flash.events.IOErrorEvent;
	import flash.geom.ColorTransform;
	import flash.net.URLRequest;

	// Loads an icon (.dds or .swf) or logo movie and fits it, centered, into
	// a square box. Paths are relative to Data\Interface\. A .dds is mounted
	// through F4SE (see F4SE.as) and loaded as img://. Loading the same path
	// again is a no-op, so a looping animation isn't restarted by a refresh.
	//
	// MountImage "succeeds" even for a file it can't find (it mounts the
	// game's shared 1x1 placeholder), so each way of writing the path (see
	// F4SE.candidates) is tried until one gives a real image.
	public class IconSlot extends Sprite
	{
		private static const WAIT:int = 2;  // frames between mounting and loading

		private var _size:Number;
		private var _loader:Loader;
		private var _path:String = "";
		private var _tint:Object = null;  // { c, a } or null
		private var _fitted:Boolean = false;
		private var _url:String = "";
		private var _image:Boolean = false;  // _url is an img:// texture
		private var _candidates:Array = [];  // { form, file } still to try
		private var _form:int = -1;          // path form of the current try
		private var _file:String = "";       // what was mounted for it
		private var _wait:int = 0;           // frames until the next load
		private static var _logged:Object = {};  // paths already reported

		public function IconSlot(a_size:Number)
		{
			super();
			_size = a_size;
			mouseEnabled = false;
			mouseChildren = false;
		}

		public function get path():String { return _path; }

		// The loaded movie's root, once loaded (null before / if it failed).
		public function get content():Object
		{
			return _loader != null && _loader.content != null ? _loader.content : null;
		}

		public function load(a_path:String):void
		{
			a_path = a_path != null ? a_path.split("\\").join("/") : "";
			if (a_path == _path) {
				return;
			}
			_path = a_path;
			stopWaiting();
			dropLoader();
			if (a_path == "") {
				return;
			}
			if (a_path.toLowerCase().substr(-4) != ".dds") {
				_image = false;
				_url = a_path;
				startLoad();
				return;
			}
			var wanted:String = a_path;
			F4SE.whenReady(function():void {
				if (_path != wanted) {
					return;  // replaced while waiting
				}
				_image = true;
				_candidates = F4SE.candidates(wanted);
				nextCandidate();
			});
		}

		// Recolors the whole icon (keeping its own transparency), or null to
		// show its own colors.
		public function set tint(a_color:Object):void
		{
			_tint = a_color;
			applyTint();
		}

		// ---- Internals ----

		private function report(a_message:String):void
		{
			if (_logged[_path] == null) {
				_logged[_path] = true;
				HUDLog.write(a_message);
			}
		}

		private function dropLoader():void
		{
			if (_loader == null) {
				return;
			}
			if (_loader.parent == this) {
				removeChild(_loader);
			}
			try {
				_loader.unloadAndStop();
			} catch (e:Error) {
			}
			_loader = null;
		}

		private function startLoad():void
		{
			dropLoader();
			_fitted = false;
			_loader = new Loader();
			// Movies send INIT; images may only send COMPLETE.
			_loader.contentLoaderInfo.addEventListener(Event.INIT, onLoaded);
			_loader.contentLoaderInfo.addEventListener(Event.COMPLETE, onLoaded);
			_loader.contentLoaderInfo.addEventListener(IOErrorEvent.IO_ERROR, onError);
			_loader.visible = false;  // shown once it's the real image
			addChild(_loader);
			_loader.load(new URLRequest(_url));
		}

		// Mounts the next path form and loads it shortly after; false when
		// every form has been tried.
		private function nextCandidate():Boolean
		{
			while (_candidates.length > 0) {
				var c:Object = _candidates.shift();
				var url:String = F4SE.mount(c.file);
				if (url != null) {
					_form = c.form;
					_file = c.file;
					_url = url;
					_wait = WAIT;
					addEventListener(Event.ENTER_FRAME, onWaitFrame);
					return true;
				}
			}
			return false;
		}

		private function stopWaiting():void
		{
			removeEventListener(Event.ENTER_FRAME, onWaitFrame);
			_wait = 0;
		}

		private function onWaitFrame(a_event:Event):void
		{
			if (--_wait > 0) {
				return;
			}
			stopWaiting();
			startLoad();
		}

		private function applyTint():void
		{
			var target:DisplayObject = _loader;
			if (target == null) {
				return;
			}
			var ct:ColorTransform = new ColorTransform();
			if (_tint != null) {
				ct.color = _tint.c;
				ct.alphaMultiplier = _tint.a;
			}
			target.transform.colorTransform = ct;
		}

		private function onLoaded(a_event:Event):void
		{
			if (_fitted || _loader == null) {
				return;
			}
			// Fit the movie's stage (not its drawn bounds, which change as it
			// animates) or the image's size into the box.
			var w:Number = _loader.contentLoaderInfo.width;
			var h:Number = _loader.contentLoaderInfo.height;
			if (!(w > 0 && h > 0) && _loader.content != null) {
				w = _loader.content.width;
				h = _loader.content.height;
			}
			if (_image && (w <= 1 || h <= 1)) {
				// The placeholder: the game didn't find the file this way.
				F4SE.forget(_file);
				if (!nextCandidate()) {
					dropLoader();
					report("couldn't find " + _path + " with any path form (only the 1x1 placeholder)");
				}
				return;
			}
			if (_image) {
				F4SE.worked(_form);
			}
			_fitted = true;
			if (_loader.content is Bitmap) {
				Bitmap(_loader.content).smoothing = true;  // icons are scaled down
			}
			var scale:Number = _size / Math.max(1, Math.max(w, h));
			_loader.scaleX = _loader.scaleY = scale;
			_loader.x = (_size - w * scale) / 2;
			_loader.y = (_size - h * scale) / 2;
			_loader.visible = true;
			applyTint();
			report("loaded " + _path + (_image && _form != 0 ? " (path form " + _form + ")" : "") + " (" + w + "x" + h + ")");
			dispatchEvent(new Event(Event.COMPLETE));
		}

		private function onError(a_event:IOErrorEvent):void
		{
			if (_image) {
				F4SE.forget(_file);
			}
			if (_image && nextCandidate()) {
				return;
			}
			dropLoader();
			report("couldn't load " + _path + (_image ? " with any path form" : ""));
		}
	}
}
