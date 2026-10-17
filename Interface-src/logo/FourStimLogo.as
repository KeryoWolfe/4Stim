package
{
	import flash.display.Bitmap;
	import flash.display.MovieClip;

	// The logo on 4Stim's MCM page (MCMLib.as): 4StimMCMLogo.png. MCM's docs
	// say its image control takes vector art only, but a picture inside a
	// movie clip shows fine (MCM tints it in the Pip-Boy color, as the rest).
	//
	// MCM's image control has no "align" and puts the image at the left of
	// the row, so the picture sits in the middle of a clear box as wide as the
	// row. Off center? Change ROW_WIDTH (MCM's stage units, 1280x720).
	public class FourStimLogo extends MovieClip
	{
		private static const ROW_WIDTH:Number = 664;
		private static const LOGO_WIDTH:Number = 600;

		[Embed(source="4StimMCMLogo.png")]
		private static const PICTURE:Class;

		public function FourStimLogo()
		{
			super();
			var picture:Bitmap = new PICTURE() as Bitmap;
			picture.smoothing = true;
			picture.scaleX = picture.scaleY = LOGO_WIDTH / picture.width;
			picture.x = (ROW_WIDTH - LOGO_WIDTH) / 2;
			graphics.beginFill(0, 0);
			graphics.drawRect(0, 0, ROW_WIDTH, picture.height + 12);
			graphics.endFill();
			addChild(picture);
		}
	}
}
