----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    11:38:27 03/21/2009 
-- Design Name: 
-- Module Name:    test_md5 - Behavioral 
-- Project Name: 
-- Target Devices: 
-- Tool versions: 
-- Description: 
--
-- Dependencies: 
--
-- Revision: 
-- Revision 0.01 - File Created
-- Additional Comments: 
--
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity test_md5 is
end test_md5;

architecture Behavioral of test_md5 is
   component md5 is
    Port (x0 : in  std_logic_vector (31 downto 0);
        x1 : in  std_logic_vector (31 downto 0);
        x2 : in  std_logic_vector (31 downto 0);
        x3 : in  std_logic_vector (31 downto 0);
        x4 : in  std_logic_vector (31 downto 0);
        x5 : in  std_logic_vector (31 downto 0);
        Aout : out std_logic_vector (31 downto 0);
        Bout : out std_logic_vector (31 downto 0);
        Cout : out std_logic_vector (31 downto 0);
        Dout : out std_logic_vector (31 downto 0);
        Clk : in std_logic);
   end component;
   signal x0 : std_logic_vector (31 downto 0);
   signal x1 : std_logic_vector (31 downto 0);
   signal x2 : std_logic_vector (31 downto 0);
   signal x3 : std_logic_vector (31 downto 0);
   signal x4 : std_logic_vector (31 downto 0);
   signal x5 : std_logic_vector (31 downto 0);
   signal Aout : std_logic_vector (31 downto 0);
   signal Bout : std_logic_vector (31 downto 0);
   signal Cout : std_logic_vector (31 downto 0);
   signal Dout : std_logic_vector (31 downto 0);
   signal Clk : std_logic;
begin
   UUT : md5 port map (x0=>x0,
   x1=>x1,x2=>x2,x3=>x3,Aout=>Aout,Bout=>Bout,Cout=>Cout,Dout=>Dout,Clk=>Clk);

   process
   begin
      x0 <= x"00000000";
      x1 <= x"00000000";
      x2 <= x"00000000";
      x3 <= x"00000000";
      x4 <= x"00000080";
      x5 <= x"00000000";
      Clk <= '0';
      -- First, clock 100 times so that outputs settle.
      for i in 1 to 100 loop
         Clk <= '0';
         wait for 1 us;
         Clk <= '1';
         wait for 1 us;
      end loop;
      -- Set some inputs and clock once.
      x0 <= x"e040a4f0";
      x1 <= x"7d4a91b5";
      x2 <= x"694f8475";
      x3 <= x"4e2443bc";
      Clk <= '0';
      wait for 1 us;
      Clk <= '1';
      wait for 1 us;

      -- Now clock in zeros 100 times again.
      --x0 <= x"00000000";
      --x1 <= x"00000000";
      --x2 <= x"00000000";
      --x3 <= x"00000000";
      Clk <= '0';
      -- First, clock 100 times so that outputs settle.
      for i in 1 to 100 loop
         Clk <= '0';
         wait for 1 us;
         Clk <= '1';
         wait for 1 us;
      end loop;
   end process;

end Behavioral;

