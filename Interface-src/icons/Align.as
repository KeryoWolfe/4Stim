package
{
	[SWF(width="64", height="64", frameRate="60")]
	public class Align extends TabIcon
	{
		public function Align()
		{
			pen();
			line(3, 6, 21, 6);
			line(3, 12, 21, 12);
			line(3, 18, 21, 18);
			dot(8, 6, 2.4);
			dot(16, 12, 2.4);
			dot(10, 18, 2.4);
		}
	}
}
