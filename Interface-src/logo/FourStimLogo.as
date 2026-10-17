package
{
	import flash.display.MovieClip;

	// The logo on 4Stim's MCM page (MCMLib.as). MCM's image control has no
	// "align", and puts the image at the left of the row, so the logo sits in
	// the middle of a clear box as wide as the row's text area (measured from
	// the page's centered title), which centers it. Off center on a page? Change
	// ROW_WIDTH (MCM's stage units, 1280x720).
	public class FourStimLogo extends MovieClip
	{
		private static const ROW_WIDTH:Number = 664;
		private static const LOGO_SIZE:Number = 200;
		private static const HEIGHT:Number = 216;  // the logo and a little room under it

		public function FourStimLogo()
		{
			super();
			graphics.beginFill(0, 0);
			graphics.drawRect(0, 0, ROW_WIDTH, HEIGHT);
			graphics.endFill();
			var art:MovieClip = new MovieClip();
			LogoArt.draw(art.graphics);
			art.scaleX = art.scaleY = LOGO_SIZE / LogoArt.SIZE;
			art.x = ROW_WIDTH / 2;
			art.y = LOGO_SIZE / 2;
			addChild(art);
		}
	}
}
