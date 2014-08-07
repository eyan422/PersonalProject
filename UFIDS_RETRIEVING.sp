create or replace
PROCEDURE UFIDS_RETRIEVING AS 

    Cursor Tabcur 
    Is 
    
    Select Max(Appn_Row_Id),Max(Row_Date_Time) 
    From Ufids_Current_Arrivals_x/*@another database*/;
  
v_stat 		VARCHAR2(4000);
V_Cur 		Integer;
v_ret     NUMBER;
PclMaxId 	ufids_current_arrivals.Appn_Row_Id%Type;
PclMaxDate 	ufids_current_arrivals.Row_date_Time%Type;
Query_Str Varchar2(2000);

BEGIN
    OPEN TabCur;
      Begin
        Fetch Tabcur Into PclMaxId,PclMaxDate;
		
		Dbms_Output.Put_Line( 'Max ID:' || Pclmaxid || '-----' || 'Max Date:' || Pclmaxdate);
    Dbms_Output.Put_Line( 'Truncate ufids_current_arrivals_x' );
    
    -- delete old content of table
    v_stat := 'TRUNCATE TABLE ufids_current_arrivals_x';
    V_Cur := Dbms_Sql.Open_Cursor;
    Dbms_Sql.Parse(V_Cur, V_Stat, Dbms_Sql.Native);
    V_Ret := Dbms_Sql.Execute(V_Cur);
    Dbms_Sql.Close_Cursor(V_Cur);
    commit;
    
    Dbms_Output.Put_Line( 'Refill ufids_current_arrivals:');
		
    If Pclmaxid Is Null
    Then
      Dbms_Output.Put_Line( 'Pclmaxid is null');
      If Pclmaxdate Is Null
      Then
      Dbms_Output.Put_Line( 'Pclmaxdate is null');
      Dbms_Output.Put_Line( 'Insert Into Ufids_Current_Arrivals_X (Select * From Ufids_X)');
      Insert Into Ufids_Current_Arrivals_X (Select * From Ufids_X);
      commit;
      Else
      Dbms_Output.Put_Line( 'Pclmaxdate is not null');
      Dbms_Output.Put_Line( 'Insert Into Ufids_Current_Arrivals_X (Select * From Ufids_X Where Row_date_Time > ' || Pclmaxdate || ')');
      Insert Into Ufids_Current_Arrivals_X (Select * From Ufids_X Where Row_date_Time > Pclmaxdate);
      commit;
      end if; 
    Else
      Dbms_Output.Put_Line( 'Pclmaxid is not null');
      If Pclmaxdate Is Null
      Then
      Dbms_Output.Put_Line( 'Pclmaxdate is null');
      Dbms_Output.Put_Line( 'Insert Into Ufids_Current_Arrivals_X (Select * From Ufids_X Where Appn_Row_Id >= ' || Pclmaxid || ')');
      Insert Into Ufids_Current_Arrivals_X (Select * From Ufids_X where Appn_Row_Id >= PclMaxId);
      Commit;
      Else
      Dbms_Output.Put_Line( 'Pclmaxdate is not null');
      Dbms_Output.Put_Line( 'Insert Into Ufids_Current_Arrivals_X (Select * From Ufids_X Where Appn_Row_Id > ' || Pclmaxid || ' or ' || 'Row_date_Time >' || Pclmaxdate || ')');
      Query_Str := 'Insert Into Ufids_Current_Arrivals_X (Select * From Ufids_X Where Appn_Row_Id >= :Pclmaxid or Row_date_Time > :Pclmaxdate)';
      Execute Immediate Query_Str Using Pclmaxid, Pclmaxdate; 
      commit;
      end if; 
    commit;
    End If;
    
    End;
    CLOSE TabCur;
    ROLLBACK;
END;