package
{
	import flash.display.Bitmap;
	import flash.display.MovieClip;

	// 4StimMCMLogo.png itself, for MCM's page (MCMLib.as), in case MCM shows
	// a picture held inside a movie clip (its docs say its image control takes
	// vector art only, and Scaleform didn't show an embedded picture in the
	// HUD before). Laid out as FourStimLogo: centered in a clear box as wide as
	// the row.
	public class FourStimLogoBitmap extends MovieClip
	{
		[Embed(source="4StimMCMLogo.png")]
		private static const PICTURE:Class;

		public function FourStimLogoBitmap()
		{
			super();
			var picture:Bitmap = new PICTURE() as Bitmap;
			picture.smoothing = true;
			var scale:Number = FourStimLogo.LOGO_WIDTH / picture.width;
			picture.scaleX = picture.scaleY = scale;
			picture.x = (FourStimLogo.ROW_WIDTH - FourStimLogo.LOGO_WIDTH) / 2;
			graphics.beginFill(0, 0);
			graphics.drawRect(0, 0, FourStimLogo.ROW_WIDTH, picture.height + 12);
			graphics.endFill();
			addChild(picture);
		}
	}
}
