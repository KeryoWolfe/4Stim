package
{
	import flash.display.MovieClip;

	// Document class of FourStimPickerMenu.swf. The actual menu is the child
	// Menu_mc (PickerPanel), found by the plugin at "root1.Menu_mc". If the
	// plugin ever links to this root instead, the list and key presses are
	// passed on to the panel, and BGSCodeObj points at the panel's.
	public class FourStimPickerMenu extends MovieClip
	{
		public var Menu_mc:PickerPanel;
		public var BGSCodeObj:Object;

		public function FourStimPickerMenu()
		{
			super();
			Menu_mc = new PickerPanel();
			Menu_mc.name = "Menu_mc";
			addChild(Menu_mc);
			BGSCodeObj = Menu_mc.BGSCodeObj;  // same object either way
		}

		public function SetScenes(a_list:Array, a_title:String):void
		{
			Menu_mc.SetScenes(a_list, a_title);
		}

		public function ProcessUserEvent(a_name:String, a_down:Boolean):Boolean
		{
			return Menu_mc.ProcessUserEvent(a_name, a_down);
		}
	}
}
