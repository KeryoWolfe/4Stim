package
{
	[SWF(width="64", height="64", frameRate="60")]
	public class Navigation extends TabIcon
	{
		public function Navigation()
		{
			pen();
			// Two arrows chasing each other round a circle.
			arc(12, 12, 8.5, -159.4, -40.2);
			line(18.5, 2.5, 18.5, 6.5);
			line(18.5, 6.5, 14.5, 6.5);
			arc(12, 12, 8.5, 20.6, 139.8);
			line(5.5, 21.5, 5.5, 17.5);
			line(5.5, 17.5, 9.5, 17.5);
		}
	}
}
