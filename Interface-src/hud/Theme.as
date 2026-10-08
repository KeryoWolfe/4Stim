package hud
{
	import flash.display.GradientType;
	import flash.display.Graphics;
	import flash.geom.Matrix;
	import flash.text.TextField;
	import flash.text.TextFormat;

	// The active theme (the parsed theme JSON the plugin sends with SetTheme),
	// plus drawing helpers that read it. Colors are "#RRGGBB" or "#RRGGBBAA";
	// list colors are one color or an array of them (a gradient).
	public class Theme
	{
		public var data:Object;

		public function Theme(a_data:Object)
		{
			data = a_data != null ? a_data : {};
		}

		// ---- Reading ----

		public function colors(a_key:String):Array
		{
			var all:Object = data.colors != null ? data.colors : {};
			var value:* = all[a_key];
			var out:Array = [];
			if (value is Array) {
				for each (var item:* in value) {
					out.push(parse(String(item)));
				}
			} else if (value != null) {
				out.push(parse(String(value)));
			}
			if (out.length == 0) {
				out.push({ c: 0xFFFFFF, a: 1 });
			}
			return out;
		}

		public function color(a_key:String):uint { return colors(a_key)[0].c; }
		public function alpha(a_key:String):Number { return colors(a_key)[0].a; }

		public function show(a_key:String):Boolean
		{
			return data.show == null || data.show[a_key] !== false;
		}

		public function layout(a_key:String, a_default:*):*
		{
			return data.layout != null && data.layout[a_key] != null ? data.layout[a_key] : a_default;
		}

		public function get font():String
		{
			return data.font != null ? String(data.font) : "$MAIN_Font";
		}

		public static function parse(a_text:String):Object
		{
			var hex:String = a_text.charAt(0) == "#" ? a_text.substr(1) : a_text;
			var c:uint = uint(parseInt(hex.substr(0, 6), 16));
			var a:Number = hex.length >= 8 ? parseInt(hex.substr(6, 2), 16) / 255 : 1;
			return { c: c, a: a };
		}

		// ---- Drawing ----

		// Fills a rectangle with a list color: one stop is a plain fill,
		// several are a gradient at a_rotation (radians; 0 = left to right,
		// PI/2 = top to bottom). a_alphas, if given, replaces the stops'
		// alphas (it is stretched or cut to the number of stops).
		public static function fill(a_g:Graphics, a_stops:Array, a_x:Number, a_y:Number, a_w:Number, a_h:Number,
			a_rotation:Number = 0, a_alphaScale:Number = 1, a_corner:Number = 0):void
		{
			begin(a_g, a_stops, a_x, a_y, a_w, a_h, a_rotation, a_alphaScale);
			if (a_corner > 0) {
				a_g.drawRoundRect(a_x, a_y, a_w, a_h, a_corner * 2, a_corner * 2);
			} else {
				a_g.drawRect(a_x, a_y, a_w, a_h);
			}
			a_g.endFill();
		}

		public static function begin(a_g:Graphics, a_stops:Array, a_x:Number, a_y:Number, a_w:Number, a_h:Number,
			a_rotation:Number = 0, a_alphaScale:Number = 1):void
		{
			if (a_stops.length == 1) {
				a_g.beginFill(a_stops[0].c, a_stops[0].a * a_alphaScale);
				return;
			}
			var cols:Array = [], alphas:Array = [], ratios:Array = [];
			for (var i:int = 0; i < a_stops.length; i++) {
				cols.push(a_stops[i].c);
				alphas.push(a_stops[i].a * a_alphaScale);
				ratios.push(Math.round(255 * i / (a_stops.length - 1)));
			}
			var m:Matrix = new Matrix();
			m.createGradientBox(a_w, a_h, a_rotation, a_x, a_y);
			a_g.beginGradientFill(GradientType.LINEAR, cols, alphas, ratios, m);
		}

		// A color from a list color at position a_t (0..1), blending stops.
		public static function sample(a_stops:Array, a_t:Number):uint
		{
			if (a_stops.length == 1) {
				return a_stops[0].c;
			}
			var pos:Number = Math.max(0, Math.min(1, a_t)) * (a_stops.length - 1);
			var i:int = Math.min(int(pos), a_stops.length - 2);
			var f:Number = pos - i;
			var a:uint = a_stops[i].c, b:uint = a_stops[i + 1].c;
			var r:uint = ((a >> 16) & 0xFF) + (((b >> 16) & 0xFF) - ((a >> 16) & 0xFF)) * f;
			var g:uint = ((a >> 8) & 0xFF) + (((b >> 8) & 0xFF) - ((a >> 8) & 0xFF)) * f;
			var bl:uint = (a & 0xFF) + ((b & 0xFF) - (a & 0xFF)) * f;
			return (r << 16) | (g << 8) | bl;
		}

		// ---- Text ----

		// Text in the theme's font. The name maps through the game's font
		// library; if it can't be found, a device font keeps the text visible.
		public function makeText(a_size:Number, a_color:uint, a_bold:Boolean = false):TextField
		{
			var field:TextField = new TextField();
			field.defaultTextFormat = new TextFormat(font, a_size, a_color, a_bold);
			field.embedFonts = true;
			field.selectable = false;
			field.mouseEnabled = false;
			field.text = "Ag";
			if (field.textWidth == 0) {
				field.embedFonts = false;
				field.defaultTextFormat = new TextFormat("_sans", a_size, a_color, a_bold);
			}
			field.text = "";
			return field;
		}
	}
}
