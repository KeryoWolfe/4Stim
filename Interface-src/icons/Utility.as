package
{
	[SWF(width="64", height="64", frameRate="60")]
	public class Utility extends TabIcon
	{
		public function Utility()
		{
			pen();
			circle(12, 12, 3.2);
			line(12, 2.5, 12, 5);
			line(12, 19, 12, 21.5);
			line(2.5, 12, 5, 12);
			line(19, 12, 21.5, 12);
			line(5.3, 5.3, 7, 7);
			line(17, 17, 18.7, 18.7);
			line(5.3, 18.7, 7, 17);
			line(17, 7, 18.7, 5.3);
		}
	}
}
