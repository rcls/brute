
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
entity adder3 is
  -- Addends with 2, 4 and 5 cycle latency to sum.
  port (addend2 : in word_t;
        addend4 : in word_t;
        addend5 : in word_t;
        Sum  : out word_t;
        Clk  : in  std_logic);
end adder3;

architecture Behavioral of adder3 is
  signal addend2_36 : std_logic_vector (35 downto 0);
  signal addend4_48 : word48_t;
  signal addend5_36 : std_logic_vector (35 downto 0);
  signal intermediate : word48_t;
  signal sum48 : word48_t;

begin
  addend2_36 <= x"0" & addend2;
  addend4_48 <= x"0000" & addend4;
  addend5_36 <= x"0" & addend5;

  Sum <= sum48 (31 downto 0);

  AddOne : DSP48A
    generic map (
      A0REG => 1,
      A1REG => 1,
      B0REG => 1,
      B1REG => 1,
      CARRYINREG => 0,
      CARRYINSEL => "OPMODE5",
      CREG => 1,
      DREG => 1,
      MREG => 1, -- Dummy, multiplier not in use.
      OPMODEREG => 0, -- Constant anyway.
      PREG => 1, -- Register output.
      RSTTYPE => "SYNC") -- Don't use it anyway.
    port map (
      BCOUT=> open,
      CARRYOUT=> open,
      P=> intermediate,
      PCOUT=> open,

      A=> addend5_36 (35 downto 18),
      B=> addend5_36 (17 downto 0),
      C=> addend4_48,
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

  AddTwo : DSP48A
    generic map (
      A0REG => 0,
      A1REG => 1,
      B0REG => 0,
      B1REG => 1,
      CARRYINREG => 0,
      CARRYINSEL => "OPMODE5",
      CREG => 1,
      DREG => 1,
      MREG => 1, -- Dummy, multiplier not in use.
      OPMODEREG => 0, -- Constant anyway.
      PREG => 1, -- Register output.
      RSTTYPE => "SYNC") -- Don't use it anyway.
    port map (
      BCOUT=> open,
      CARRYOUT=> open,
      P=> Sum48,
      PCOUT=> open,

      A=> addend2_36 (35 downto 18),
      B=> addend2_36 (17 downto 0),
      C=> intermediate,
      CARRYIN=> '0',
      CEA=> '1',
      CEB=> '1',
      CEC=> '1',
      CECARRYIN=> '0', -- Carry in reg not used.
      CED=> '0',
      CEM=> '0',
      CEOPMODE=> '0',
      CEP=> '1',
      CLK=> Clk,
      D=> "00" & x"0000",
      OPMODE=> "00001111", -- add/add / carry0/no-pre-add / C port / DBA
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
