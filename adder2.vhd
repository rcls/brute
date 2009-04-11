
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
library UNISIM;
use UNISIM.VComponents.all;

library work;
use work.defs.all;

-- Compute (OneA + OneB) + Two.
entity adder2 is
  -- Addends with 1 and 3 cycle latency to sum.
  port (addend1 : in word_t;
        addend3 : in word_t;
        Sum  : out word_t;
        Clk  : in  std_logic);
end adder2;

architecture Behavioral of adder2 is
  signal A : std_logic_vector (17 downto 0);
  signal B : std_logic_vector (17 downto 0);
  signal C : word48_t;
  signal D : std_logic_vector (17 downto 0);
  signal sum48 : word48_t;

begin
  B <= addend3 (17 downto 0);
  A <= "0000" & addend3 (31 downto 18);
  D <= "00" & x"0000";
  C <= x"0000" & addend1;

  Sum <= sum48 (31 downto 0);

  Add : DSP48A
    generic map (
      A0REG => 1,
      A1REG => 1,
      B0REG => 1,
      B1REG => 1,
      CARRYINREG => 0,
      CARRYINSEL => "OPMODE5",
      CREG => 0,
      DREG => 0,
      MREG => 1, -- Dummy, multiplier not in use.
      OPMODEREG => 0, -- Constant anyway.
      PREG => 1, -- Register output.
      RSTTYPE => "SYNC") -- Don't use it anyway.
    port map (
      BCOUT=> open,
      CARRYOUT=> open,
      P=> sum48,
      PCOUT=> open,

      A=> A,
      B=> B,
      C=> C,
      CARRYIN=> '0',
      CEA=> '1',
      CEB=> '1',
      CEC=> '1',
      CECARRYIN=>'0', -- Carry in reg not used.
      CED=> '0',
      CEM=> '0',
      CEOPMODE=> '0',
      CEP=> '1',
      CLK=> Clk,
      D=> "00" & x"0000",
      OPMODE=> "00001111", -- add/add / carry0/no-pre-add / C port/DBA
      PCIN=> x"000000000000",
      RSTA=> '0',
      RSTB=> '0',
      RSTC=> '0',
      RSTCARRYIN=> '0',
      RSTD=> '0',
      RSTM=> '0',
      RSTP=> '0',
      RSTOPMODE=> '0');

end Behavioral;
