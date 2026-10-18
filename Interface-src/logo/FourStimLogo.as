package
{
	import flash.display.Bitmap;
	import flash.display.MovieClip;

	// The logo on 4Stim's MCM page (MCMLib.as): 4StimMCMLogo.png. MCM's docs
	// say its image control takes vector art only, but a picture inside a
	// movie clip shows fine (MCM tints it in the Pip-Boy color, as the rest).
	//
	// MCM's image control has no "align" and puts the image at the top left
	// of the page, so the picture sits in a clear box: centered across it (as
	// wide as the row) and TOP_MARGIN down from the top, which centers it on
	// the page (measured from a screenshot). Off center? Change ROW_WIDTH or
	// TOP_MARGIN (MCM's stage units, 1280x720).
	public class FourStimLogo extends MovieClip
	{
		private static const ROW_WIDTH:Number = 664;
		private static const LOGO_WIDTH:Number = 600;
		private static const TOP_MARGIN:Number = 120;

		[Embed(source="4StimMCMLogo.png")]
		private static const PICTURE:Class;

		public function FourStimLogo()
		{
			super();
			var picture:Bitmap = new PICTURE() as Bitmap;
			picture.smoothing = true;
			picture.scaleX = picture.scaleY = LOGO_WIDTH / picture.width;
			picture.x = (ROW_WIDTH - LOGO_WIDTH) / 2;
			picture.y = TOP_MARGIN;
			graphics.beginFill(0, 0);
			graphics.drawRect(0, 0, ROW_WIDTH, TOP_MARGIN + picture.height + 12);
			graphics.endFill();
			addChild(picture);
		}
	}
}
