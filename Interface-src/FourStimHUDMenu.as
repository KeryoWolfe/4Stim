package
{
	import flash.display.MovieClip;
	import hud.F4SE;
	import hud.HUDPanel;

	// Document class of FourStimHUDMenu.swf, the default in-scene HUD. The
	// HUD itself is the child Menu_mc (HUDPanel), found by the plugin at
	// "root1.Menu_mc". If the plugin links to this root instead, every API
	// call is passed on to the panel, and BGSCodeObj is the panel's.
	public class FourStimHUDMenu extends MovieClip
	{
		public var Menu_mc:HUDPanel;
		public var BGSCodeObj:Object;

		public function FourStimHUDMenu()
		{
			super();
			Menu_mc = new HUDPanel();
			Menu_mc.name = "Menu_mc";
			addChild(Menu_mc);
			BGSCodeObj = Menu_mc.BGSCodeObj;  // same object either way
		}

		// Called by F4SE with its Scaleform functions (root.f4se); icons
		// use its MountImage to show .dds files.
		public function onF4SEObjCreated(a_f4se:Object):void { F4SE.attach(a_f4se); }

		public function GetApiVersion():int { return Menu_mc.GetApiVersion(); }
		public function SetScreen(a_left:Number, a_top:Number, a_right:Number, a_bottom:Number):void { Menu_mc.SetScreen(a_left, a_top, a_right, a_bottom); }
		public function SetTheme(a_theme:Object):void { Menu_mc.SetTheme(a_theme); }
		public function SetScene(a_scene:Object):void { Menu_mc.SetScene(a_scene); }
		public function SetActors(a_actors:Array):void { Menu_mc.SetActors(a_actors); }
		public function SetMeters(a_values:Array):void { Menu_mc.SetMeters(a_values); }
		public function SetSpeed(a_level:int, a_count:int):void { Menu_mc.SetSpeed(a_level, a_count); }
		public function SetNavigation(a_entries:Array):void { Menu_mc.SetNavigation(a_entries); }
		public function SetUtility(a_entries:Array):void { Menu_mc.SetUtility(a_entries); }
		public function SetFocus(a_focused:Boolean):void { Menu_mc.SetFocus(a_focused); }
		public function SetAlign(a_data:Object):void { Menu_mc.SetAlign(a_data); }
		public function SetPaused(a_paused:Boolean):void { Menu_mc.SetPaused(a_paused); }
		public function PlayClimax(a_strength:Number):void { Menu_mc.PlayClimax(a_strength); }
		public function ProcessUserEvent(a_name:String, a_down:Boolean):Boolean { return Menu_mc.ProcessUserEvent(a_name, a_down); }
	}
}
