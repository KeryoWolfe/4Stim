package hud
{
	// Writes to 4Stim.log through the plugin (BGSCodeObj.Log). Messages from
	// before the plugin's functions are attached (icons start loading as
	// soon as the first data arrives) are kept and sent once they are.
	public class HUDLog
	{
		private static var _codeObj:Object = null;
		private static var _queue:Array = [];

		public static function attach(a_codeObj:Object):void
		{
			_codeObj = a_codeObj;
			flush();
		}

		public static function write(a_message:String):void
		{
			_queue.push(a_message);
			flush();
		}

		private static function flush():void
		{
			if (_codeObj == null || _codeObj.Log == null) {
				return;
			}
			while (_queue.length > 0) {
				_codeObj.Log(String(_queue.shift()));
			}
		}
	}
}
