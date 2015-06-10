package colorpencil;

class Pencil {}

class ColorPencil extends Pencil {
	String color;
	ColorPencil(String color) 
	{
		this.color = color;
	}
	
	public static void main(String agrs[])
	{
		ColorPencil var1 = new ColorPencil("RED");
		Pencil var2 = new ColorPencil("BLUE");
		
		System.out.println(var1.color);
		System.out.println( ((ColorPencil)var2).color );
	}
}


