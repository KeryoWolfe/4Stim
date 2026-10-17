package
{
	import flash.display.MovieClip;

	// MCM's image library for 4Stim's page: Data\MCM\Config\4Stim\lib.swf.
	// MCM loads it and shows the class an "image" control names by its
	// className (config.json: "libName": "4Stim", "className": "FourStimLogo").
	// This root class only has to reference that class so mxmlc puts it in
	// the movie.
	[SWF(width="256", height="256", frameRate="60", backgroundColor="#000000")]
	public class MCMLib extends MovieClip
	{
		private static const CLASSES:Array = [FourStimLogo];

		public function MCMLib()
		{
			super();
		}
	}
}
