library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

-- cost table 94 gets closure but with 2.5hour PAR times!
-- ct2 gets 20ps, ct86 gets 7ps.

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
library UNISIM;
use UNISIM.VComponents.all;

library work;
use work.defs.all;

entity control is
  port (Clk_125MHz : in std_logic;
        LED : out byte_t);
end control;

architecture Behavioral of control is

  component Clock
    port (CLKIN_IN : in STD_LOGIC; CLKFX_OUT : out STD_LOGIC);
  end component;

  signal jtag_tck : std_logic;
  signal jtag_tms : std_logic;
  signal jtag_capture : std_logic;
  signal jtag_drck1 : std_logic;
  signal jtag_drck2 : std_logic;
  signal jtag_sel1 : std_logic;
  signal jtag_sel2 : std_logic;
  signal jtag_shift : std_logic;
  signal jtag_tdi : std_logic;
  signal jtag_update : std_logic;
  signal jtag_tdo1 : std_logic;
  signal jtag_tdo2 : std_logic;

  component feeder is
    port (hit_read_addr : in byte_t;
          hit_ram_o : out word144_t;
          hit_idx : out byte_t;
          global_count : in word48_t;
          global_count_match : in std_logic;
          load_data : in word96_t;
          load_match : in std_logic;
          sample_match : in std_logic;
          Clk : in std_logic);
  end component;

  component counter is
    port (count : out word48_t;
          match : in word48_t;
          hit : out std_logic;
          clk : in std_logic);
  end component;

  signal Clk : std_logic;

  signal A_next : word96_t;           -- 3 input words to md5.
  signal B_next : word96_t;
  signal A_output : word128_t;        -- 4 output words from md5.
  signal B_output : word128_t;

  -- 152 bit shift register attached to user1.
  -- 8 bit opcode / 48 bit clock / 96 bits data  or
  -- 8 bit opcode / 8 bit address.
  -- Opcode is bitmask:
  -- bit 0 : read result + 8 bit address (pipeline A)
  -- bit 1 : load md5 - 48 bit clock count + 96 bits data. (pipeline A)
  -- bit 2 : sample md5 - 48 bit clock count (pipeline A)
  -- bit 4 : read result + 8 bit address (pipeline B)
  -- bit 5 : load md5 - 48 bit clock count + 96 bits data. (pipeline B)
  -- bit 6 : sample md5 - 48 bit clock count (pipeline B)
  signal command : std_logic_vector (151 downto 0);
  alias command_address : byte_t is command (143 downto 136);
  alias command_data : word96_t is command (95 downto 0);

  signal command_valid : std_logic := '0';

  alias A_op_readram : std_logic is command (144);
  alias A_op_load : std_logic is command (145);
  alias A_op_sample : std_logic is command (146);

  alias B_op_readram : std_logic is command (148);
  alias B_op_load : std_logic is command (149);
  alias B_op_sample : std_logic is command (150);

  -- Detect rising edge with "01" and falling by "10"; we're crossing
  -- clock domains.
  signal command_edge : std_logic_vector (1 downto 0) := "00";

  -- The 48 bit global cycle counter.
  signal global_count : word48_t;
  signal global_count_latch : word48_t;
  signal global_count_match : std_logic; -- Does global count match command?

  signal A_load_match : std_logic; -- Buffered load command hit.
  signal B_load_match : std_logic; -- Buffered load command hit.
  signal A_sample_match : std_logic; -- Buffered sample command hit.
  signal B_sample_match : std_logic; -- Buffered sample command hit.
  signal A_hitram_o : word144_t;
  signal B_hitram_o : word144_t;
  signal A_hit_idx : byte_t;
  signal B_hit_idx : byte_t;

begin

  BSCAN_SPARTAN3_inst : BSCAN_SPARTAN3A
    port map (
      TCK => jtag_tck,
      TMS => jtag_tms,
      CAPTURE => jtag_capture, -- CAPTURE output from TAP controller
      DRCK1 => jtag_drck1,     -- Data register output for USER1 functions
      DRCK2 => jtag_drck2,        -- Data register output for USER2 functions
      --RESET => jtag_RESET,        -- Reset output from TAP controller
      SEL1 => jtag_sel1,          -- USER1 active output
      SEL2 => jtag_sel2,          -- USER2 active output
      SHIFT => jtag_shift,        -- SHIFT output from TAP controller
      TDI => jtag_tdi,            -- TDI output from TAP controller
      UPDATE => jtag_update,      -- UPDATE output from TAP controller
      TDO1 => jtag_tdo1,          -- Data input for USER1 function
      TDO2 => jtag_tdo2           -- Data input for USER2 function
      );

  clock_inst : clock port map (
    CLKFX_OUT => Clk,
    CLKIN_IN => Clk_125MHz);

  -- The outputs; the jtag unit seems to take care of latching on falling TCK.
  jtag_tdo1 <= command (0);

  process (jtag_tck)
  begin
    if jtag_tck'event and jtag_tck = '1' then
      if jtag_sel1 = '1' then
        if jtag_shift = '1' then
          -- Shift in the command.
          command <= jtag_tdi & command (151 downto 1);
        end if;
        if jtag_capture = '1' then
          command_valid <= '0';
          -- If the previous command read ram then grab the data out of the
          -- hit-ram.  Else grab the latched clock.
          if A_op_readram = '1' then
            command <= x"00" & A_hitram_o;
          elsif B_op_readram = '1' then
            command <= x"00" & B_hitram_o;
          else
            command <= x"00000000000000000000000000" & global_count_latch;
          end if;
        elsif jtag_update = '1' then
          command_valid <= '1';
        end if;
      end if;
    end if;
  end process;

  global_cnt : counter
    port map (count => global_count,
              match => command (143 downto 96),
              hit => global_count_match,
              Clk => Clk);

  feedA : feeder
    port map (hit_read_addr => command_address,
              hit_ram_o     => A_hitram_o,
              hit_idx       => A_hit_idx,
              global_count_match => global_count_match,
              global_count  => global_count,
              load_data     => command_data,
              load_match    => A_load_match,
              sample_match  => A_sample_match,
              Clk           => Clk);

  feedB : feeder
    port map (hit_read_addr => command_address,
              hit_ram_o     => B_hitram_o,
              hit_idx       => B_hit_idx,
              global_count_match => global_count_match,
              global_count  => global_count,
              load_data     => command_data,
              load_match    => B_load_match,
              sample_match  => B_sample_match,
              Clk           => Clk);

  -- Nice LEDs
  LED <= A_hit_idx + B_hit_idx;

  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      -- Run the command_valid edge.  This serves both edge detection and
      -- as a guard against metastability.
      command_edge(0) <= command_valid;
      command_edge(1) <= command_edge(0);

      -- On command_valid rising, latch the global counter.
      if command_edge = "01" then
        global_count_latch <= global_count;
      end if;

      -- Buffer the load-match, sample-match and hit.  The write takes place
      -- the cycle after the load mux, so be careful about that.
      A_load_match <= command_edge(1) and global_count_match and A_op_load;
      B_load_match <= command_edge(1) and global_count_match and B_op_load;
      A_sample_match <= command_edge(1) and global_count_match and A_op_sample;
      B_sample_match <= command_edge(1) and global_count_match and B_op_sample;
    end if;
  end process;

end Behavioral;
