
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

package defs is

  -- Number of bits to truncate MD5 to.
  constant bits : integer := 72;

  subtype nibble_t is std_logic_vector (3 downto 0);
  subtype byte_t is std_logic_vector (7 downto 0);
  subtype word_t is std_logic_vector (31 downto 0);

  subtype word48_t is std_logic_vector (47 downto 0);
  subtype word96_t is std_logic_vector (95 downto 0);
  subtype word128_t is std_logic_vector (127 downto 0);
  subtype word144_t is std_logic_vector (143 downto 0);

  type dataset_t is array (natural range <>) of word_t;

  function hexify (n : nibble_t) return byte_t;
  function hexbyte (data : word96_t; n : integer) return byte_t;
  function maskb0 (n : integer) return byte_t;
  function maskb1 (n : integer) return byte_t;

  function hexword (data : word96_t; n : integer) return word_t;
  function maskw0 (n : integer) return word_t;
  function maskw1 (n : integer) return word_t;

  function rotl (x : word_t; n : integer) return word_t;

end defs;


package body defs is
  function hexify (n : nibble_t) return byte_t is
    variable alpha : std_logic := n(3) and (n(2) or n(1));
    variable result : byte_t;
  begin
    case n is
      when x"1"=> return x"31";
      when x"2"=> return x"32";
      when x"3"=> return x"33";
      when x"4"=> return x"34";
      when x"5"=> return x"35";
      when x"6"=> return x"36";
      when x"7"=> return x"37";
      when x"8"=> return x"38";
      when x"9"=> return x"39";
      when x"a"=> return x"61";
      when x"b"=> return x"62";
      when x"c"=> return x"63";
      when x"d"=> return x"64";
      when x"e"=> return x"65";
      when x"f"=> return x"66";
      when others => return x"30";
    end case;
  end hexify;

  function hexbyte (data : word96_t; n : integer) return byte_t is
	  variable masked : word96_t;
  begin
     masked := x"000000000000000000000000";
	  masked(bits - 1 downto 0) := data (bits - 1 downto 0);
	  if n * 4 < bits then
	    return hexify (masked (n * 4 + 3 downto n * 4));
	  elsif n * 4 < bits + 4 then
	    return x"80";
	  else
	    return x"00";
	  end if;
  end hexbyte;

  function maskb0 (n : integer) return byte_t is
  begin
    if n * 4 < bits then
	   return x"20";
	 elsif n * 4 < bits + 4 then
	   return x"80";
	 else
	   return x"00";
	 end if;
  end;

  function maskb1 (n : integer) return byte_t is
  begin
    if n * 4 < bits then
	   return x"7f";
	 elsif n * 4 < bits + 4 then
	   return x"80";
	 else
	   return x"00";
	 end if;
  end;

  function hexword (data : word96_t; n : integer) return word_t is
  begin
    return hexbyte (data, 4 * n + 3)
	    &   hexbyte (data, 4 * n + 2)
	    &   hexbyte (data, 4 * n + 1)
	    &   hexbyte (data, 4 * n);
  end;

  function maskw0 (n : integer) return word_t is
  begin
    return maskb0 (4 * n + 3)
	    &   maskb0 (4 * n + 2)
	    &   maskb0 (4 * n + 1)
	    &   maskb0 (4 * n);
  end;

  function maskw1 (n : integer) return word_t is
  begin
    return maskb1 (4 * n + 3)
	    &   maskb1 (4 * n + 2)
	    &   maskb1 (4 * n + 1)
	    &   maskb1 (4 * n);
  end;

  function rotl (x : word_t; n : integer) return word_t is
  begin
    if n = 0 then
      return x;
    else
      return x(31 - n downto 0) & x(31 downto 31 - n + 1);
    end if;
  end rotl;

end package body defs;
