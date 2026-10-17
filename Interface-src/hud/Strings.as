package hud
{
	// The plugin's translations (Data\Interface\Translations\4Stim_<lang>.txt,
	// docs/TRANSLATIONS.md), sent with SetStrings. Every label the movies
	// write themselves asks here, with its English as the fallback.
	public class Strings
	{
		private static var _table:Object = {};

		public static function set(a_table:Object):void
		{
			_table = a_table != null ? a_table : {};
		}

		public static function t(a_key:String, a_english:String):String
		{
			var value:* = _table[a_key];
			return value != undefined && value != null && String(value) != "" ? String(value) : a_english;
		}
	}
}
