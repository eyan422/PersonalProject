package inheritancestudy;

class person
{
	void name()
	{
		System.out.println("base class");
	}
}

class frank extends person
{
	void name()
	{
		System.out.println("frank class");
	}
}

public class InheritanceStudy {

	public static void main(String args[])
	{
		person p = new person();
		frank f = new frank();
		
		person pa[] = new person[2];
		
		pa[0] = p;
		pa[1] = f;
		
		for(person m : pa)
		{
			if ( m instanceof frank )
			{
				//System.out.println("frank");
				m.name();
			}
			else if ( m instanceof person )
			{
				//System.out.println("person");
				m.name();
			}
		}
	}
}
